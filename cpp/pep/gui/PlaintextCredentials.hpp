#pragma once
#ifdef _WIN32

#include <pep/utils/Win32Api.hpp>

namespace pep {


namespace win32api {

class PlaintextCredentials {
private:
  pep::win32api::SecureBuffer<WCHAR> mUserName;
  pep::win32api::SecureBuffer<WCHAR> mPassword;
  pep::win32api::SecureBuffer<WCHAR> mDomain;

  PlaintextCredentials();

  static PlaintextCredentials FromAuthenticationBuffer(PVOID buffer, DWORD bufferSize);
  static void DiscardAuthenticationBuffer(PVOID pvAuthBuffer, ULONG ulAuthBufferSize);

public:
  static PlaintextCredentials FromPrompt(HWND parentWindow, const std::string& caption, const std::string& message);

  void runCommandLine(const std::string& cmdLine) const;
};

}


}
#endif
