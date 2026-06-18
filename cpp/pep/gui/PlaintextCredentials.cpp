#ifdef _WIN32
#include <pep/gui/PlaintextCredentials.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <string_view>

#include <objbase.h>
#include <wincred.h>

#pragma comment(lib, "Credui")  // Cred*


namespace {
const std::string LogTag("Plaintext credentials");
}

namespace pep {


namespace win32api {

PlaintextCredentials::PlaintextCredentials()
  : userName_(CREDUI_MAX_USERNAME_LENGTH + 1), password_(CREDUI_MAX_PASSWORD_LENGTH + 1), domain_(CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1) {
}

/* If the buffer contains credentials produced by CredUIPromptForWindowsCredentialsW, and the
 * user entered "user@domain.suffix" then the domain name is not extracted from that information.
 * Instead the userName_ will contain the complete "user@domain.suffix", and the domain_
 * will be empty.
 */
PlaintextCredentials PlaintextCredentials::FromAuthenticationBuffer(PVOID buffer, DWORD bufferSize) {
  PlaintextCredentials result;
  auto userNameChars = static_cast<DWORD>(result.userName_.getMaxItems());
  auto passwordChars = static_cast<DWORD>(result.password_.getMaxItems());
  auto domainChars = static_cast<DWORD>(result.domain_.getMaxItems());
  // https://flylib.com/books/en/1.286.1.88/1/
  BOOL fOK = ::CredUnPackAuthenticationBufferW(
    CRED_PACK_PROTECTED_CREDENTIALS,
    buffer,
    bufferSize,
    result.userName_.getAddress(),
    &userNameChars,
    result.domain_.getAddress(),
    &domainChars,
    result.password_.getAddress(),
    &passwordChars);
  if (!fOK) {
    pep::win32api::ApiCallFailure::RaiseLastError();
  }
  return result;
}

void PlaintextCredentials::DiscardAuthenticationBuffer(PVOID pvAuthBuffer, ULONG ulAuthBufferSize) {
  if (pvAuthBuffer != nullptr) {
    pep::win32api::ClearMemory(pvAuthBuffer, size_t{ulAuthBufferSize});
    ::CoTaskMemFree(pvAuthBuffer);
  }
}

PlaintextCredentials PlaintextCredentials::FromPrompt(HWND parentWindow, const std::string& caption, const std::string& message) {
  auto wideCaption = pep::win32api::Utf8StringToWide(caption);
  auto wideMessage = pep::win32api::Utf8StringToWide(message);

  BOOL fSave = FALSE;
  CREDUI_INFOW cui;
  cui.cbSize = sizeof(cui);
  cui.hwndParent = parentWindow;
  cui.pszMessageText = wideMessage.c_str();
  cui.pszCaptionText = wideCaption.c_str();
  cui.hbmBanner = nullptr;
  ULONG ulAuthPkg = 0;
  LPVOID pvOutAuthBuffer = nullptr;
  ULONG ulOutAuthBufferSize = 0U;
  auto promptError = ::CredUIPromptForWindowsCredentialsW( // https://docs.microsoft.com/en-us/windows/win32/api/wincred/nf-wincred-creduipromptforwindowscredentialsw
    &cui,                         // CREDUI_INFOW structure
    0,                            // Reason
    &ulAuthPkg,                   // authentication package for which the credentials in the pvInAuthBuffer buffer are serialized
    nullptr,                      // credential BLOB that is used to populate the credential fields in the dialog box
    0U,                           // size, in bytes, of the pvInAuthBuffer buffer: 0 since we're passing nullptr
    &pvOutAuthBuffer,             // receives the credential BLOB produced by CredUIPromptForWindowsCredentialsW
    &ulOutAuthBufferSize,         // receives the size of the credential BLOB produced by CredUIPromptForWindowsCredentialsW
    &fSave,                       // State of save check box
    CREDUIWIN_GENERIC             // flags
  );
  PEP_DEFER(DiscardAuthenticationBuffer(pvOutAuthBuffer, ulOutAuthBufferSize));
  if (promptError != NO_ERROR) {
    // TODO: deal with promptError == 1223 (user cancelled)
    throw std::runtime_error("Credential prompt returned error " + std::to_string(promptError));
  }
  return FromAuthenticationBuffer(pvOutAuthBuffer, ulOutAuthBufferSize);
}

void PlaintextCredentials::runCommandLine(const std::string& cmdLine) const {
  // Default to passing the raw member values to CreateProcessWithLogonW
  LPCWSTR passedUserName = userName_.getAddress();
  LPCWSTR passedDomain = domain_.getAddress();

  // But if our userName_ contains a "domain\username" specification, split them for the call to CreateProcessWithLogonW (below)
  std::vector<std::wstring> parts;
  boost::split(parts, std::wstring(userName_.getAddress()), std::bind_front(std::equal_to{}, L'\\'));

  switch (parts.size()) {
  case 0: // Empty user name: pass as specified. Presumably the call to CreateProcessWithLogonW will return an error.
    break;
  case 1: // Not a "domain\username" but "user@domain.tld" or a simple user name.
    /* According to https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createprocesswithlogonw :
     * "If you use the UPN format, user@DNS_domain_name, the lpDomain parameter must be nullptr."
     */
    if (std::wstring_view(passedUserName).find(L'@') != std::wstring_view::npos) {
      if (!std::wstring_view(passedDomain).empty()) {
        throw std::runtime_error("Cannot specify both a user@DNS_domain_name and a separate domain name");
      }
      passedDomain = nullptr;
    }
    break;
  case 2: // Split "domain\username" into separate fields
    if (passedDomain[0] != L'\0') {
      throw std::runtime_error("Cannot specify both a domain\\username and a separate domain name");
    }
    PEP_LOG(LogTag, Severity::Info) << "Splitting domain\\username specification '" << userName_.getAddress() << "' into separate fields";
    passedDomain = parts[0].c_str();
    passedUserName = parts[1].c_str();
    break;
  default:
    throw std::runtime_error("More than one domain\\username delimiter in specified user name");
  }

  PEP_LOG(LogTag, Severity::Info) << "Running command line as user '" << passedUserName << "' on domain '" << passedDomain << "'";

  auto wideCmdLine = pep::win32api::Utf8StringToWide(cmdLine);

  // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createprocesswithlogonw
  PROCESS_INFORMATION pi{};
  STARTUPINFOW si{};
  si.cb = sizeof(STARTUPINFOW);
  /* CreateProcessWithLogonW works for domain account information entered as "user@domain.suffix",
   * even though userName_ will contain that entire string and domain_ will be empty.
   */
  if (::CreateProcessWithLogonW(passedUserName,
                                passedDomain,
                                password_.getAddress(),
                                LOGON_WITH_PROFILE,
                                nullptr,
                                wideCmdLine.data(),
                                NORMAL_PRIORITY_CLASS,
                                nullptr,
                                nullptr,
                                &si,
                                &pi)
      == 0) {
    pep::win32api::ApiCallFailure::RaiseLastError();
  }
}

}


}
#endif // _WIN32
