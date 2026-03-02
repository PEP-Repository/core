//NOLINTBEGIN This file can be upgraded a lot in the future, see also https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2279

#if defined(WITH_UNWINDER) && !defined(_WIN32)
// Define for libunwind to only allow local unwinding
# define UNW_LOCAL_ONLY
#endif

//C Includes
#include <pep/application/Unwinder.hpp>
#include <pep/utils/Log.hpp>

// TODO rename this macro and/or revise this source's logic, since libunwind and its functionality is not available/used on Windows
#ifdef WITH_UNWINDER

#include <pep/utils/Platform.hpp>
#include <pep/utils/LocalSettings.hpp>
#ifndef _WIN32
#include <libunwind.h>
#include <sys/utsname.h>
#include <cxxabi.h>
#include <dirent.h>
#elif _WIN32
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp")
#endif /*_WIN32*/
#include <fstream>
#include <cstdio>
#include <cassert>
#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <cinttypes>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <cerrno>
#include <sstream>

namespace pep {

static std::string szCrashReportFileName;
static const std::string LOG_TAG ("Unwinder");

#ifndef _WIN32

static const char* SignalNumberToString (int dwSignalNumber) {
  switch (dwSignalNumber) {
  case SIGHUP:
    return "SIGHUP";
  case SIGINT:
    return "SIGINT";
  case SIGQUIT:
    return "SIGQUIT";
  case SIGILL:
    return "SIGILL";
  case SIGTRAP:
    return "SIGTRAP";
  case SIGABRT:
    return "SIGABRT";
#if SIGABRT != SIGIOT
  case SIGIOT:
    return "SIGIOT";
#endif
  case SIGBUS:
    return "SIGBUS";
  case SIGFPE:
    return "SIGFPE";
  case SIGKILL:
    return "SIGKILL";
  case SIGUSR1:
    return "SIGUSR1";
  case SIGSEGV:
    return "SIGSEGV";
  case SIGUSR2:
    return "SIGUSR2";
  case SIGPIPE:
    return "SIGPIPE";
  case SIGALRM:
    return "SIGALRM";
  case SIGTERM:
    return "SIGTERM";
#ifdef SIGSTKFLT
  case SIGSTKFLT:
    return "SIGSTKFLT";
#endif
  case SIGCHLD:
    return "SIGCHLD";
  case SIGCONT:
    return "SIGCONT";
  case SIGSTOP:
    return "SIGSTOP";
  case SIGTSTP:
    return "SIGTSTP";
  case SIGTTIN:
    return "SIGTTIN";
  case SIGTTOU:
    return "SIGTTOU";
  case SIGURG:
    return "SIGURG";
  case SIGXCPU:
    return "SIGXCPU";
  case SIGXFSZ:
    return "SIGXFSZ";
  case SIGVTALRM:
    return "SIGVTALRM";
  case SIGPROF:
    return "SIGPROF";
  case SIGWINCH:
    return "SIGWINCH";
  case SIGIO:
    return "SIGIO";
#ifdef SIGPWR
  case SIGPWR:
    return "SIGPWR";
#endif
  case SIGSYS:
    return "SIGSYS";
#if defined(SIGUNUSED) && SIGSYS != SIGUNUSED
  case SIGUNUSED:
    return "SIGUNUSED";
#endif
  default:
    return "UNKNOWN";
  }
}

struct SignalCodeStrings {
  const char* szShort;
  const char* szLong;
};

static SignalCodeStrings SignalCodeToString (int dwSignalNumber, int dwSignalCode) {
  switch (dwSignalNumber) {
  case SIGILL:
    switch (dwSignalCode) {
    case ILL_ILLOPC:
      return {"ILL_ILLOPC", "Illegal opcode."};
    case ILL_ILLOPN:
      return {"ILL_ILLOPN", "Illegal operand."};
    case ILL_ILLADR:
      return {"ILL_ILLADR", "Illegal addressing mode."};
    case ILL_ILLTRP:
      return {"ILL_ILLTRP", "Illegal trap."};
    case ILL_PRVOPC:
      return {"ILL_PRVOPC", "Privileged opcode."};
    case ILL_PRVREG:
      return {"ILL_PRVREG", "Privileged register."};
    case ILL_COPROC:
      return {"ILL_COPROC", "Coprocessor error."};
    case ILL_BADSTK:
      return {"ILL_BADSTK", "Internal stack error."};

    default:
      return {"UNKNOWN", "Unknown"};
    }
  case SIGFPE:
    switch (dwSignalCode) {
    case FPE_INTDIV:
      return {"FPE_INTDIV", "Integer divide-by-zero."};
    case FPE_INTOVF:
      return {"FPE_INTOVF", "Integer overflow."};
    case FPE_FLTDIV:
      return {"FPE_FLTDIV", "Floating point divide-by-zero."};
    case FPE_FLTOVF:
      return {"FPE_FLTOVF", "Floating point overflow."};
    case FPE_FLTUND:
      return {"FPE_FLTUND", "Floating point underflow."};
    case FPE_FLTRES:
      return {"FPE_FLTRES", "Floating point inexact result."};
    case FPE_FLTINV:
      return {"FPE_FLTINV", "Invalid floating point operation."};
    case FPE_FLTSUB:
      return {"FPE_FLTSUB", "Subscript out of range."};

    default:
      return {"UNKNOWN", "Unknown"};
    }
  case SIGSEGV:
    switch (dwSignalCode) {
    case SEGV_MAPERR:
      return {"SEGV_MAPERR", "Address not mapped."};
    case SEGV_ACCERR:
      return {"SEGV_ACCERR", "Invalid permissions."};

    default:
      return {"UNKNOWN", "Unknown"};
    }
  case SIGBUS:
    switch (dwSignalCode) {
    case BUS_ADRALN:
      return {"BUS_ADRALN", "Invalid address alignment."};
    case BUS_ADRERR:
      return {"BUS_ADRERR", "Non-existent physical address."};
    case BUS_OBJERR:
      return {"BUS_OBJERR", "Object-specific hardware error."};


    default:
      return {"UNKNOWN", "Unknown"};
    }
  case SIGTRAP:
    switch (dwSignalCode) {
    case TRAP_BRKPT:
      return {"TRAP_BRKPT", "Process breakpoint.    "};
    case TRAP_TRACE:
      return {"TRAP_TRACE", "Process trace trap."};

    default:
      return {"UNKNOWN", "Unknown"};
    }
  case SIGCHLD:
    switch (dwSignalCode) {
    case CLD_EXITED:
      return {"CLD_EXITED", "Child has exited."};


    case CLD_KILLED:
      return {"CLD_KILLED", "Child has terminated abnormally and did not create a core file."};
    case CLD_DUMPED:
      return {"CLD_DUMPED", "Child has terminated abnormally and created a core file."};
    case CLD_TRAPPED:
      return {"CLD_TRAPPED", "Traced child has trapped."};
    case CLD_STOPPED:
      return {"CLD_STOPPED", "Child has stopped."};
    case CLD_CONTINUED:
      return {"CLD_CONTINUED", "Stopped child has continued."};

    default:
      return {"UNKNOWN", "Unknown"};
    }
  case SIGIO:
//         case SIGPOLL: // same as SIGIO
    switch (dwSignalCode) {
    case POLL_IN:
      return {"POLL_IN", "Data input available."};
    case POLL_OUT:
      return {"POLL_OUT", "Output buffers available."};
    case POLL_MSG:
      return {"POLL_MSG", "Input message available."};
    case POLL_ERR:
      return {"POLL_ERR", "I/O error."};
    case POLL_PRI:
      return {"POLL_PRI", "High priority input available."};
    case POLL_HUP:
      return {"POLL_HUP", "Device disconnected."};

    default:
      return {"UNKNOWN", "Unknown"};
    }

  default:
    switch (dwSignalCode) {
#if defined(SI_NOINFO)
    case SI_NOINFO:
      return {"SI_NOINFO", "The NuTCRACKER Platform was unable to determine complete signal information."};
#endif
    case SI_USER:
      return {"SI_USER", "Signal sent by kill(), pthread_kill(), raise(), abort() or alarm()."};

    case SI_QUEUE:
      return {"SI_QUEUE", "Signal was sent by sigqueue()."};

    case SI_TIMER:
      return {"SI_TIMER", "Signal was generated by expiration of a timer set by timer_settimer()."};
    case SI_ASYNCIO:
      return {"SI_ASYNCIO", "Signal was generated by completion of an asynchronous I/O request."};
    case SI_MESGQ:
      return {"SI_MESGQ", "Signal was generated by arrival of a message on an empty message queue."};

    default:
      return {"UNKNOWN", "Unknown"};
    }
  }
}

static const char* lpDaysOfTheWeek[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* lpMonthsOfTheYear[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

[[noreturn]] static void BacktraceSignalHandler(int dwSignalNumber, siginfo_t* lpSignalInfo, void* lpArguments) {

  //Setup output file
  FILE* lpCrashReportFile = fopen (szCrashReportFileName.c_str(), "w");
  if (lpCrashReportFile == nullptr) {
    fprintf(stderr, "Error opening crash report file %s: %s \n", szCrashReportFileName.c_str(), strerror(errno));

    // We specifically do not throw an exception or runtime error here
    // Because that will result in SIGABRT, which results in this handler being called again
    // while the filesystem is somehow not allowing us to open a file for writing
    exit(EXIT_FAILURE);
  }

  SignalCodeStrings stSignalCodeStrings = SignalCodeToString (dwSignalNumber, lpSignalInfo->si_code);
  fprintf (lpCrashReportFile, "Caught Signal: %s (%d)\n", SignalNumberToString(dwSignalNumber), dwSignalNumber);
  fprintf (lpCrashReportFile, "Signal Code: %s (%s, %d)\n", stSignalCodeStrings.szShort, stSignalCodeStrings.szLong, lpSignalInfo->si_code);

  switch (dwSignalNumber) {
  case SIGILL:
  case SIGFPE:
  case SIGSEGV:
  case SIGBUS:
    fprintf (lpCrashReportFile, "Faulting address: 0x%0*" PRIxPTR "\n", static_cast<int>(sizeof(void*) * 2), reinterpret_cast<uintptr_t>(lpSignalInfo->si_addr));
    break;
  }

  auto unixSeconds = time(nullptr);
  struct tm stTimeDisect;
  gmtime_r(&unixSeconds, &stTimeDisect);
  fprintf (lpCrashReportFile, "Time: %s, %02d %s %04d %02d:%02d:%02d UTC (Unix %llu)\n",
           lpDaysOfTheWeek[stTimeDisect.tm_wday],
           stTimeDisect.tm_mday,
           lpMonthsOfTheYear[stTimeDisect.tm_mon],
           stTimeDisect.tm_year + 1900,
           stTimeDisect.tm_hour,
           stTimeDisect.tm_min,
           stTimeDisect.tm_sec,
           static_cast<unsigned long long>(unixSeconds)
          );

  //----------------------
  //Begin function unwind section
  //----------------------
  fprintf (lpCrashReportFile, ">--- Stack Trace ---<\n");
  unw_cursor_t cursor;
  unw_context_t uc;
  //architecture independent "registers"
  unw_word_t ip;
  //x64 general purpose registers
//   unw_word_t RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP, R8, R9, R10, R11, R12, R13, R14, R15;

  unw_getcontext (&uc);
  unw_init_local (&cursor, &uc);

  unsigned int i = 0;
  while (unw_step(&cursor) > 0) {
//     unw_get_proc_name (&cursor, function_name, 256, &offp);
//     unw_get_proc_info(&cursor, &pip);
    unw_get_reg (&cursor, UNW_REG_IP, &ip);
//     unw_get_reg (&cursor, UNW_REG_SP, &sp);
//     unw_get_reg(&cursor, UNW_X86_64_RAX, &RAX);
//     unw_get_reg(&cursor, UNW_X86_64_RBX, &RBX);
//     unw_get_reg(&cursor, UNW_X86_64_RCX, &RCX);
//     unw_get_reg(&cursor, UNW_X86_64_RDX, &RDX);
//     unw_get_reg(&cursor, UNW_X86_64_RSI, &RSI);
//     unw_get_reg(&cursor, UNW_X86_64_RDI, &RDI);
//     unw_get_reg(&cursor, UNW_X86_64_RBP, &RBP);
//     unw_get_reg(&cursor, UNW_X86_64_RSP, &RSP);
//     unw_get_reg(&cursor, UNW_X86_64_R8, &R8);
//     unw_get_reg(&cursor, UNW_X86_64_R9, &R9);
//     unw_get_reg(&cursor, UNW_X86_64_R10, &R10);
//     unw_get_reg(&cursor, UNW_X86_64_R11, &R11);
//     unw_get_reg(&cursor, UNW_X86_64_R12, &R12);
//     unw_get_reg(&cursor, UNW_X86_64_R13, &R13);
//     unw_get_reg(&cursor, UNW_X86_64_R14, &R14);
//     unw_get_reg(&cursor, UNW_X86_64_R15, &R15);
//     fprintf (lpCrashReportFile, "============== %s ==============\n", function_name);
    //printf ("ip = %lx, sp = %lx\n", (long) ip, (long) sp);

    char szSymbolName[256];
    char *symbolName = szSymbolName;
    unw_word_t offset;
    memset (szSymbolName, 0, sizeof (szSymbolName));
    unw_get_proc_name(&cursor, szSymbolName, sizeof(szSymbolName), &offset);
    szSymbolName [sizeof(szSymbolName) - 1] = '\0';

    // attempt to demangle symbol name to C++ source name
    // if demangling fails, we fall back to the symbol name
    int dwDemangleStatus;
    char *lpStackTraceSymbolName = abi::__cxa_demangle(szSymbolName, nullptr, nullptr, &dwDemangleStatus);
    if (dwDemangleStatus == 0 && lpStackTraceSymbolName != nullptr) {
      symbolName = lpStackTraceSymbolName;
    }

    fprintf (lpCrashReportFile, "#%-2u 0x%0*" PRIxPTR " in %s+0x%lu\n", i, static_cast<int>(sizeof(void*) * 2), static_cast<uintptr_t>(ip), symbolName, offset);

    // free the malloc'd result of abi::__cxa_demangle from above
    free(lpStackTraceSymbolName);

//     fprintf (lpCrashReportFile, "Symbol: %s, Instruction pointer (IP): %lx, Stack pointer: %lx\nStart IP: %llx, End IP: %llx, LSDA: %llx, Handler: %llx, Format: %u, Unwind Info Size: %u, Unwind Info: %llx, Extra: %llx\n", stack_symbol, (long)ip, (long)sp, pip.start_ip, pip.end_ip, pip.lsda, pip.handler, pip.format, pip.unwind_info_size, pip.unwind_info, pip.extra);
//     fprintf (lpCrashReportFile, "General register states:\nRAX = %llx, RBX = %llx, RCX = %llx, RDX = %llx, RSI = %llx, RDI = %llx, RBP = %llx, RSP = %llx,\nR8 = %llx, R9 = %llx, R10 = %llx, R11 = %llx, R12 = %llx, R13 = %llx, R14 = %llx, R15 = %llx\n", RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP, R8, R9, R10, R11, R12, R13, R14, R15);

    i++;
  }

#if defined(__linux__) && (defined (__x86_64__) || defined(__i386__) || defined(__arm__))
  const ucontext_t* lpContext = static_cast<ucontext_t*>(lpArguments);
  fprintf (lpCrashReportFile, ">--- Register Dump ---<\n");
#if defined ( __x86_64__)
  fprintf (lpCrashReportFile, "      R8 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_R8]));
  fprintf (lpCrashReportFile, "      R9 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_R9]));
  fprintf (lpCrashReportFile, "     R10 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_R10]));
  fprintf (lpCrashReportFile, "     R11 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_R11]));
  fprintf (lpCrashReportFile, "     R12 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_R12]));
  fprintf (lpCrashReportFile, "     R13 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_R13]));
  fprintf (lpCrashReportFile, "     R14 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_R14]));
  fprintf(lpCrashReportFile, "     R15 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_R15]));
  fprintf(lpCrashReportFile, "     RDI : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RDI]));
  fprintf(lpCrashReportFile, "     RSI : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RSI]));
  fprintf(lpCrashReportFile, "     RBP : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RBP]));
  fprintf(lpCrashReportFile, "     RBX : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RBX]));
  fprintf(lpCrashReportFile, "     RDX : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RDX]));
  fprintf(lpCrashReportFile, "     RAX : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RAX]));
  fprintf(lpCrashReportFile, "     RCX : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RCX]));
  fprintf(lpCrashReportFile, "     RSP : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RSP]));
  fprintf(lpCrashReportFile, "     RIP : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_RIP]));
  fprintf(lpCrashReportFile, "     EFL : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_EFL]));
  fprintf(lpCrashReportFile, "  CSGSFS : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_CSGSFS]));
  fprintf(lpCrashReportFile, "     ERR : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_ERR]));
  fprintf(lpCrashReportFile, "  TRAPNO : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_TRAPNO]));
  fprintf(lpCrashReportFile, " OLDMASK : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_OLDMASK]));
  fprintf (lpCrashReportFile, "     CR2 : %016" PRIx64 "\n", static_cast<std::uint64_t>(lpContext->uc_mcontext.gregs[REG_CR2]));
#elif defined (__i386__)
  fprintf (lpCrashReportFile, "      GS : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_GS]));
  fprintf (lpCrashReportFile, "      FS : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_FS]));
  fprintf (lpCrashReportFile, "      ES : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_ES]));
  fprintf (lpCrashReportFile, "      DS : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_DS]));
  fprintf (lpCrashReportFile, "     EDI : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_EDI]));
  fprintf (lpCrashReportFile, "     ESI : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_ESI]));
  fprintf (lpCrashReportFile, "     EBP : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_EBP]));
  fprintf (lpCrashReportFile, "     ESP : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_ESP]));
  fprintf (lpCrashReportFile, "     EBX : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_EBX]));
  fprintf (lpCrashReportFile, "     EDX : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_EDX]));
  fprintf (lpCrashReportFile, "     ECX : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_ECX]));
  fprintf (lpCrashReportFile, "     EAX : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_EAX]));
  fprintf (lpCrashReportFile, "  TRAPNO : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_TRAPNO]));
  fprintf (lpCrashReportFile, "     ERR : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_ERR]));
  fprintf (lpCrashReportFile, "     EIP : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_EIP]));
  fprintf (lpCrashReportFile, "      CS : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_CS]));
  fprintf (lpCrashReportFile, "     EFL : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_EFL]));
  fprintf (lpCrashReportFile, "    UESP : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_UESP]));
  fprintf (lpCrashReportFile, "      SS : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.gregs[REG_SS]));
#elif defined (__arm__)
  fprintf (lpCrashReportFile, "      R0 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r0));
  fprintf (lpCrashReportFile, "      R1 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r1));
  fprintf (lpCrashReportFile, "      R2 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r2));
  fprintf (lpCrashReportFile, "      R3 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r3));
  fprintf (lpCrashReportFile, "      R4 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r4));
  fprintf (lpCrashReportFile, "      R5 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r5));
  fprintf (lpCrashReportFile, "      R6 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r6));
  fprintf (lpCrashReportFile, "      R7 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r7));
  fprintf (lpCrashReportFile, "      R8 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r8));
  fprintf (lpCrashReportFile, "      R9 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r9));
  fprintf (lpCrashReportFile, "     R10 : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_r10));
  fprintf (lpCrashReportFile, "      FP : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_fp));
  fprintf (lpCrashReportFile, "      IP : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_ip));
  fprintf (lpCrashReportFile, "      SP : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_sp));
  fprintf (lpCrashReportFile, "      LR : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_lr));
  fprintf (lpCrashReportFile, "      PC : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_pc));
  fprintf (lpCrashReportFile, "    CPSR : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.arm_cpsr));
  fprintf (lpCrashReportFile, "  TRAPNO : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.trap_no));
  fprintf (lpCrashReportFile, "     ERR : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.error_code));
  fprintf (lpCrashReportFile, " OLDMASK : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.oldmask));
  fprintf (lpCrashReportFile, "   FAULT : %08" PRIx32 "\n", static_cast<std::uint32_t>(lpContext->uc_mcontext.fault_address));
#endif
#endif /* defined(__linux__) && (defined (__x86_64__) || defined(__i386__) || defined(__arm__)) */
  // printf("-----------------------\n");
  fprintf (lpCrashReportFile, ">--- System Parameters ---<\n");

  //----------------------
  //End function unwind section
  //----------------------

  //----------------------
  //Begin environmental variables scraping
  //----------------------
  fprintf (lpCrashReportFile,
           "User : %s\n"
           "Working directory : %s\n"
           "PATH : %s\n"
           "BUILD_TARGET: %s\n"
           "BUILD_REF: %s\n"
           "BUILD_COMMIT: %s\n",
           getenv("USER"),
           getenv("PWD"),
           getenv("PATH"),
           BUILD_TARGET,
           BUILD_REF,
           BUILD_COMMIT
          );
  // printf("User: {%s}, Working directory: {%s}, PATH {%s}\n",getenv("USER"), getenv("PWD"), getenv("PATH"));
  //----------------------
  //End environmental variables scraping
  //----------------------

  //----------------------
  //Begin Memory information section
  //----------------------
  uint64_t pages = static_cast<uint64_t>(sysconf(_SC_PHYS_PAGES));
  uint64_t page_size = static_cast<uint64_t>(sysconf(_SC_PAGE_SIZE));
  fprintf (lpCrashReportFile, "System Memory in bytes: %lu\n", pages*page_size);
  //----------------------
  //End Memory information section
  //----------------------

  //----------------------
  //Begin Operating System section
  //----------------------
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/uname.html
  // For documentation on the uname function and associated struct
  struct utsname system_version_buffer;
  if (uname(&system_version_buffer)>-1) {
    fprintf (lpCrashReportFile,
             "System Name: %s\n"
             "System Release: %s\n"
             "System Version: %s\n"
             "System uArch: %s\n"
             "Network Node: %s\n",
             system_version_buffer.sysname,
             system_version_buffer.release,
             system_version_buffer.version,
             system_version_buffer.machine,
             system_version_buffer.nodename
            );
    // printf("System Name: %s\nSystem Release: %s\nSystem Version: %s\nSystem uArch: %s\nNetwork Node: %s\n",system_version_buffer.sysname,system_version_buffer.release,system_version_buffer.version,system_version_buffer.machine,system_version_buffer.nodename);
  }

  //----------------------
  //End Operating System section
  //----------------------

  //----------------------
  //Begin PEP specifics section
  //----------------------

  //----------------------
  //End PEP specifics section
  //----------------------

  fclose(lpCrashReportFile);
  exit(2);
}

#else /* _WIN32 */
[[noreturn]] static void WindowsSignalHandler (int32_t dwSignalNumber) {
  HANDLE dumpfile = ::CreateFileA(szCrashReportFileName.c_str(),
                                  GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ,
                                  0, CREATE_ALWAYS, 0, 0);
  BOOL dumpSuccessful = ::MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                            dumpfile, MiniDumpWithDataSegs, nullptr, nullptr, nullptr);
  if (!dumpSuccessful) {
    HRESULT hError = HRESULT_FROM_WIN32(GetLastError());
    LOG(LOG_TAG, error) << "Failed to write minidump: " << hError;
  }

  exit(2);
}
#endif

void InitializeUnwinder() {
  auto szReportName = GetOutputBasePath().string();
  assert(szReportName.length() < 200);

  /*
   * TODO: send the crash dumps upstream in windows
   */

#ifndef _WIN32
  const std::string szReportDirectoryTmp = szReportName;
  const std::string szReportFileName = basename(std::string(szReportDirectoryTmp).data());
  const std::string szReportDirectoryPath = std::string(dirname(std::string(szReportDirectoryTmp).data())) + "/";

  if (DIR* lpReportDirectory = opendir (szReportDirectoryPath.c_str()); lpReportDirectory == nullptr) {
    LOG(LOG_TAG, warning) << "Unable to look through directory " << szReportDirectoryPath << " for previous crash reports: " << strerror(errno);
  } else {
    std::unique_ptr<LocalSettings>& lpLocalSettings = LocalSettings::getInstance();
    struct dirent* lpDirEntry;
    struct stat mFileStat;
    std::string szPreviousTimestamp;
    std::string szPropertyName = std::string ("LatestCrashReportTimestamp_") + szReportFileName;
    long long qwPreviousTimestamp = -1;
    long long qwLatestTimestamp = -1;
    if (lpLocalSettings->retrieveValue (&szPreviousTimestamp, "Unwinder", szPropertyName) != false) {
      qwLatestTimestamp = qwPreviousTimestamp = atoll (szPreviousTimestamp.c_str());
    }

    const std::string findPrefix = szReportFileName + '_';
    while ((lpDirEntry = readdir (lpReportDirectory)) != nullptr) {
      std::string_view fileName(lpDirEntry->d_name);
      if (!fileName.starts_with(findPrefix)) {
        /* file name must start with same prefix */
        continue;
      } else if (!fileName.ends_with("_CrashReport.txt")) {
        continue;
      }

      if (stat ((szReportDirectoryPath + "/" + lpDirEntry->d_name).c_str(), &mFileStat) != 0) {
        /* stat should always succeed
         * but there may be a race condition in which the file has been deleted at this point
         */
        continue;
      } else if ((mFileStat.st_mode & S_IFMT) != S_IFREG) {
        /* must be a regular file */
        continue;
      }

#ifdef __linux__
      long long qwTimestamp =
        mFileStat.st_mtim.tv_sec * 1000 +
        mFileStat.st_mtim.tv_nsec / 1000000;
#else
      long long qwTimestamp = mFileStat.st_mtime * 1000;
#endif

      if (qwTimestamp > qwPreviousTimestamp) {
        std::ifstream mCrashReportStream (szReportDirectoryPath + "/" + lpDirEntry->d_name);
        std::string szLine;
        if (mCrashReportStream.is_open ()) {
          LOG(LOG_TAG, debug) << "[*] ==== Crash report from previous run ====";
          while (getline (mCrashReportStream, szLine)) {
            LOG(LOG_TAG, debug) << szLine;
          }
          mCrashReportStream.close();
          LOG(LOG_TAG, debug) << "[*] ==== End Crash report from previous run ====";
        }

        if (qwTimestamp > qwLatestTimestamp) {
          qwLatestTimestamp = qwTimestamp;
        }
      }
    }

    closedir(lpReportDirectory);

    if (qwLatestTimestamp > qwPreviousTimestamp) {
      std::string szLatestTimestamp = std::to_string(qwLatestTimestamp);
      lpLocalSettings->storeValue ("Unwinder", szPropertyName, szLatestTimestamp);
      lpLocalSettings->flushChanges ();
    }
  }
#endif

  // Compute filename, no need to check for uniqueness as it includes the current time
  auto in_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::ostringstream ss;
  ss << szReportName << "_" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d-%H-%M-%S");
#ifndef _WIN32
  ss << "_CrashReport.txt";
#else
  ss << "_Crash.dmp";
#endif
  szCrashReportFileName = std::move(ss).str();

#ifndef _WIN32
  struct sigaction stSignalDescriptor;
  memset (&stSignalDescriptor, 0, sizeof(struct sigaction));
  stSignalDescriptor.sa_sigaction = BacktraceSignalHandler;
  sigemptyset (&stSignalDescriptor.sa_mask);
  stSignalDescriptor.sa_flags = SA_SIGINFO;

  sigaction (SIGILL, &stSignalDescriptor, nullptr);
  sigaction (SIGFPE, &stSignalDescriptor, nullptr);
  sigaction (SIGBUS, &stSignalDescriptor, nullptr);
  sigaction (SIGABRT, &stSignalDescriptor, nullptr);
  sigaction (SIGSEGV, &stSignalDescriptor, nullptr);
#else /* _WIN32 */
  // Catch all signals that windows will allow for
  // https://msdn.microsoft.com/en-us/library/xdkz3x12.aspx
  // This is a subset of all *nix signals
  std::signal(SIGFPE, WindowsSignalHandler);
  std::signal(SIGILL, WindowsSignalHandler);
  std::signal(SIGABRT, WindowsSignalHandler);
  std::signal(SIGSEGV, WindowsSignalHandler);
//   std::signal(SIGTERM, WindowsSignalHandler);

  // Do not attempt to handle Ctrl+C on Windows, since our handler will run in a separate thread:
  // see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2008 . Instead we'll let default handling
  // do its thing, which is (apparently) faster. This way the application won't be able to do
  // much anymore while it's being terminated.
  // Also I don't think we need (our handler to produce) a minidump when a user kills our app.
  //std::signal(SIGINT, WindowsSignalHandler);
#endif
}

}

#else

namespace pep {
  static const std::string LOG_TAG ("Unwinder");
  void InitializeUnwinder() {
    LOG(LOG_TAG, warning) << "InitializeUnwinder called even though USE_UNWINDER is not set";
  }
}


#endif

//NOLINTEND
