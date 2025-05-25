#include "pch.h"
#include "common.h"

#ifdef WIL_ENABLE_EXCEPTIONS
#include <wil/stl.h>
#endif

// Just verify that Tracelogging.h compiles.
#define PROVIDER_CLASS_NAME TestProvider
#include "TraceLoggingTests.h"

/* bd2bf191-ac1a-4732-b563-bb3e3006f617 */
TRACELOGGING_DEFINE_PROVIDER(                                                      // defines g_hProvider
    g_tlgProvider,                                                                 // Name of the provider variable
    "MyProvider",                                                                  // Human-readable name of the provider
    (0xbd2bf191, 0xac1a, 0x4732, 0xb5, 0x63, 0xbb, 0x3e, 0x30, 0x06, 0xf6, 0x17)); // Provider GUID

TEST_CASE("TraceLogging for Views", "[tracelogging]")
{
#ifdef WIL_ENABLE_EXCEPTIONS
    std::string_view sv = "kittens";
    std::wstring_view wsv = L"kittens";
    std::string s = "puppies";
    std::wstring ws = L"puppies";
    wil::zstring_view zv = "hamsters";
    wil::zwstring_view zwv = L"hamsters";

    std::string_view const& rsv = sv;
    std::wstring_view const& rwsv = wsv;
    wil::zstring_view const& rzv = zv;
    wil::zwstring_view const& rzwv = zwv;

    TraceLoggingRegister(g_tlgProvider);

    TraceLoggingWrite(
        g_tlgProvider,
        "Hello",
        TraceLoggingStringView(sv, "sv"),
        TraceLoggingWideStringView(wsv, "wsv"),
        TraceLoggingStringView(s, "s"),
        TraceLoggingWideStringView(ws, "ws"),
        TraceLoggingStringView(zv, "zv"),
        TraceLoggingWideStringView(zwv, "zwv"));

    TraceLoggingWrite(
        g_tlgProvider,
        "There",
        TraceLoggingValue(sv, "sv"),
        TraceLoggingValue(wsv, "wsv"),
        TraceLoggingValue(s, "s"),
        TraceLoggingValue(ws, "ws"),
        TraceLoggingValue(zv, "zv"),
        TraceLoggingValue(zwv, "zwv"));

    TraceLoggingWrite(
        g_tlgProvider,
        "HelloRefs",
        TraceLoggingStringView(rsv, "sv"),
        TraceLoggingWideStringView(rwsv, "wsv"),
        TraceLoggingStringView(rzv, "zv"),
        TraceLoggingWideStringView(rzwv, "zwv"));

    TraceLoggingWrite(
        g_tlgProvider,
        "ThereRefs",
        TraceLoggingValue(rsv, "sv"),
        TraceLoggingValue(rwsv, "wsv"),
        TraceLoggingValue(rzv, "s"),
        TraceLoggingValue(rzwv, "ws"));

    TraceLoggingUnregister(g_tlgProvider);
#endif
}