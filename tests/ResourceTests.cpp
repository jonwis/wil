#include "pch.h"

// Included first and then again later to ensure that we're able to "light up" new functionality based off new includes
#include <wil/resource.h>

#include <wil/com.h>
#include <wil/stl.h>

// Headers to "light up" functionality in resource.h
#include <memory>
#include <shared_mutex>
#include <roapi.h>
#include <winstring.h>
#include <WinUser.h>

#include <wil/resource.h> // NOLINT(readability-duplicate-include): Intentional
#include <wrl/implements.h>

#include "common.h"

TEST_CASE("ResourceTests::TestLastErrorContext", "[resource][last_error_context]")
{
    // Destructing the last_error_context restores the error.
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(0);
    }
    REQUIRE(GetLastError() == 42);

    // The context can be moved.
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(0);
        {
            auto another_error42 = wil::last_error_context(std::move(error42));
            SetLastError(1);
        }
        REQUIRE(GetLastError() == 42);
        SetLastError(0);
        // error42 has been moved-from and should not do anything at destruction.
    }
    REQUIRE(GetLastError() == 0);

    // The context can be self-assigned, which has no effect.
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(0);
        error42 = std::move(error42);
        SetLastError(1);
    }
    REQUIRE(GetLastError() == 42);

    // The context can be dismissed, which cause it to do nothing at destruction.
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(0);
        error42.release();
        SetLastError(1);
    }
    REQUIRE(GetLastError() == 1);

    // The value in the context is unimpacted by other things changing the last error
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(1);
        REQUIRE(error42.value() == 42);
    }
}

TEST_CASE("ResourceTests::TestScopeExit", "[resource][scope_exit]")
{
    int count = 0;
    auto validate = [&](int expected) {
        REQUIRE(count == expected);
        count = 0;
    };

    {
        auto foo = wil::scope_exit([&] {
            count++;
        });
    }
    validate(1);

    {
        auto foo = wil::scope_exit([&] {
            count++;
        });
        foo.release();
        foo.reset();
    }
    validate(0);

    {
        auto foo = wil::scope_exit([&] {
            count++;
        });
        foo.reset();
        foo.reset();
        validate(1);
    }
    validate(0);

#ifdef WIL_ENABLE_EXCEPTIONS
    {
        auto foo = wil::scope_exit_log(WI_DIAGNOSTICS_INFO, [&] {
            count++;
            THROW_HR(E_FAIL);
        });
    }
    validate(1);

    {
        auto foo = wil::scope_exit_log(WI_DIAGNOSTICS_INFO, [&] {
            count++;
            THROW_HR(E_FAIL);
        });
        foo.release();
        foo.reset();
    }
    validate(0);

    {
        auto foo = wil::scope_exit_log(WI_DIAGNOSTICS_INFO, [&] {
            count++;
            THROW_HR(E_FAIL);
        });
        foo.reset();
        foo.reset();
        validate(1);
    }
    validate(0);
#endif // WIL_ENABLE_EXCEPTIONS
}

interface __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20b00")) ITest : public IUnknown
{
    STDMETHOD_(void, Test)() = 0;
};

class __declspec(empty_bases) PointerTestObject
    : witest::AllocatedObject,
      public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>, ITest>
{
public:
    STDMETHOD_(void, Test)() {};
};

TEST_CASE("ResourceTests::TestOperationsOnGenericSmartPointerClasses", "[resource]")
{
#ifdef WIL_ENABLE_EXCEPTIONS
    {
        // wil::unique_any_t example
        wil::unique_event ptr2(wil::EventOptions::ManualReset);
        // wil::com_ptr
        wil::com_ptr<PointerTestObject> ptr3 = Microsoft::WRL::Make<PointerTestObject>();
        // wil::shared_any_t example
        wil::shared_event ptr4(wil::EventOptions::ManualReset);
        // wistd::unique_ptr example
        auto ptr5 = wil::make_unique_failfast<POINT>();

        static_assert(wistd::is_same<typename wil::smart_pointer_details<decltype(ptr2)>::pointer, HANDLE>::value, "type-mismatch");
        static_assert(wistd::is_same<typename wil::smart_pointer_details<decltype(ptr3)>::pointer, PointerTestObject*>::value, "type-mismatch");

        auto raw2 = wil::detach_from_smart_pointer(ptr2);
        auto raw3 = wil::detach_from_smart_pointer(ptr3);
        // auto raw4 = wil::detach_from_smart_pointer(ptr4); // wil::shared_any_t and std::shared_ptr do not support release().
        HANDLE raw4{};
        auto raw5 = wil::detach_from_smart_pointer(ptr5);

        REQUIRE((!ptr2 && !ptr3));
        REQUIRE((raw2 && raw3));

        wil::attach_to_smart_pointer(ptr2, raw2);
        wil::attach_to_smart_pointer(ptr3, raw3);
        wil::attach_to_smart_pointer(ptr4, raw4);
        wil::attach_to_smart_pointer(ptr5, raw5);

        raw2 = nullptr;
        raw3 = nullptr;
        raw4 = nullptr;
        raw5 = nullptr;

        wil::detach_to_opt_param(&raw2, ptr2);
        wil::detach_to_opt_param(&raw3, ptr3);

        REQUIRE((!ptr2 && !ptr3));
        REQUIRE((raw2 && raw3));

        wil::attach_to_smart_pointer(ptr2, raw2);
        wil::attach_to_smart_pointer(ptr3, raw3);
        raw2 = nullptr;
        raw3 = nullptr;

        wil::detach_to_opt_param(&raw2, ptr2);
        wil::detach_to_opt_param(&raw3, ptr3);
        REQUIRE((!ptr2 && !ptr3));
        REQUIRE((raw2 && raw3));

        [&](decltype(raw2)* ptr) {
            *ptr = raw2;
        }(wil::out_param(ptr2));
        [&](decltype(raw3)* ptr) {
            *ptr = raw3;
        }(wil::out_param(ptr3));
        [&](decltype(raw4)* ptr) {
            *ptr = raw4;
        }(wil::out_param(ptr4));
        [&](decltype(raw5)* ptr) {
            *ptr = raw5;
        }(wil::out_param(ptr5));

        REQUIRE((ptr2 && ptr3));

        // Validate R-Value compilation
        wil::detach_to_opt_param(&raw2, decltype(ptr2){});
        wil::detach_to_opt_param(&raw3, decltype(ptr3){});
    }
#endif

    std::unique_ptr<int> ptr1(new int(1));
    Microsoft::WRL::ComPtr<PointerTestObject> ptr4 = Microsoft::WRL::Make<PointerTestObject>();

    static_assert(wistd::is_same<typename wil::smart_pointer_details<decltype(ptr1)>::pointer, int*>::value, "type-mismatch");
    static_assert(wistd::is_same<typename wil::smart_pointer_details<decltype(ptr4)>::pointer, PointerTestObject*>::value, "type-mismatch");

    auto raw1 = wil::detach_from_smart_pointer(ptr1);
    auto raw4 = wil::detach_from_smart_pointer(ptr4);

    REQUIRE((!ptr1 && !ptr4));
    REQUIRE((raw1 && raw4));

    wil::attach_to_smart_pointer(ptr1, raw1);
    wil::attach_to_smart_pointer(ptr4, raw4);

    REQUIRE((ptr1 && ptr4));

    raw1 = nullptr;
    raw4 = nullptr;

    int** pNull = nullptr;
    wil::detach_to_opt_param(pNull, ptr1);
    REQUIRE(ptr1);

    wil::detach_to_opt_param(&raw1, ptr1);
    wil::detach_to_opt_param(&raw4, ptr4);

    REQUIRE((!ptr1 && !ptr4));
    REQUIRE((raw1 && raw4));

    [&](decltype(raw1)* ptr) {
        *ptr = raw1;
    }(wil::out_param(ptr1));
    [&](decltype(raw4)* ptr) {
        *ptr = raw4;
    }(wil::out_param(ptr4));

    REQUIRE((ptr1 && ptr4));

    raw1 = wil::detach_from_smart_pointer(ptr1);
    [&](int** ptr) {
        *ptr = raw1;
    }(wil::out_param_ptr<int**>(ptr1));
    REQUIRE(ptr1);
}

// NOLINTNEXTLINE(misc-use-internal-linkage): Compilation only test...
void StlAdlTest()
{
    // This test has exposed some Argument Dependent Lookup issues in wistd / stl.  Primarily we're
    // just looking for clean compilation.

    std::vector<wistd::unique_ptr<int>> vec;
    vec.emplace_back(new int{1});
    vec.emplace_back(new int{2});
    vec.emplace_back(new int{3});
    std::rotate(begin(vec), begin(vec) + 1, end(vec));

    REQUIRE(*vec[0] == 1);
    REQUIRE(*vec[1] == 3);
    REQUIRE(*vec[2] == 2);

    decltype(vec) vec2;
    vec2 = std::move(vec);
    REQUIRE(*vec2[0] == 1);
    REQUIRE(*vec2[1] == 3);
    REQUIRE(*vec2[2] == 2);

    decltype(vec) vec3;
    std::swap(vec2, vec3);
    REQUIRE(*vec3[0] == 1);
    REQUIRE(*vec3[1] == 3);
    REQUIRE(*vec3[2] == 2);
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
// NOLINTNEXTLINE(misc-use-internal-linkage): Compilation only test...
void UniqueProcessInfo()
{
    wil::unique_process_information process;
    CreateProcessW(nullptr, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, nullptr, &process);
    ResumeThread(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    wil::unique_process_information other(wistd::move(process));
}
#endif

#ifdef WIL_ENABLE_EXCEPTIONS
// NOLINTNEXTLINE(misc-use-internal-linkage): Compilation only test...
void NoexceptConstructibleTest()
{
    using BaseStorage = wil::details::unique_storage<wil::details::handle_resource_policy>;

    struct ThrowingConstructor : BaseStorage
    {
        ThrowingConstructor() = default;
        explicit ThrowingConstructor(HANDLE) __WI_NOEXCEPT_(false)
        {
        }
    };

    struct ProtectedConstructor : BaseStorage
    {
    protected:
        ProtectedConstructor() = default;
        explicit ProtectedConstructor(HANDLE) WI_NOEXCEPT
        {
        }
    };

    // wil::unique_handle is one of the many types which are expected to be noexcept
    // constructible since they don't perform any "advanced" initialization.
    static_assert(wistd::is_nothrow_default_constructible_v<wil::unique_handle>, "wil::unique_any_t should always be nothrow default constructible");
    static_assert(wistd::is_nothrow_constructible_v<wil::unique_handle, HANDLE>, "wil::unique_any_t should be noexcept if the storage is");

    // The inverse: A throwing storage constructor.
    static_assert(
        wistd::is_nothrow_default_constructible_v<wil::unique_any_t<ThrowingConstructor>>,
        "wil::unique_any_t should always be nothrow default constructible");
    static_assert(
        !wistd::is_nothrow_constructible_v<wil::unique_any_t<ThrowingConstructor>, HANDLE>,
        "wil::unique_any_t shouldn't be noexcept if the storage isn't");

    // With a protected constructor wil::unique_any_t will be unable to correctly
    // "forward" the noexcept attribute, but the code should still compile.
    wil::unique_any_t<ProtectedConstructor> value{INVALID_HANDLE_VALUE};
}
#endif

struct FakeComInterface
{
    void AddRef()
    {
        refs++;
    }
    void Release()
    {
        refs--;
    }

    HRESULT __stdcall Close()
    {
        closes++;
        return S_OK;
    }

    size_t refs = 0;
    size_t closes = 0;

    bool called()
    {
        auto old = closes;
        closes = 0;
        return (old > 0);
    }

    bool has_ref() const
    {
        return (refs > 0);
    }
};

static void __stdcall CloseFakeComInterface(FakeComInterface* fake)
{
    fake->Close();
}

using unique_fakeclose_call = wil::unique_com_call<FakeComInterface, decltype(&CloseFakeComInterface), CloseFakeComInterface>;

TEST_CASE("ResourceTests::VerifyUniqueComCall", "[resource][unique_com_call]")
{
    unique_fakeclose_call call1;
    unique_fakeclose_call call2;

    // intentional compilation errors
    // unique_fakeclose_call call3 = call1;
    // call2 = call1;

    FakeComInterface fake1;
    unique_fakeclose_call call4(&fake1);
    REQUIRE(fake1.has_ref());

    unique_fakeclose_call call5(wistd::move(call4));
    REQUIRE(!call4);
    REQUIRE(call5);
    REQUIRE(fake1.has_ref());

    call4 = wistd::move(call5);
    REQUIRE(call4);
    REQUIRE(!call5);
    REQUIRE(fake1.has_ref());
    REQUIRE(!fake1.called());

    FakeComInterface fake2;
    {
        unique_fakeclose_call scoped(&fake2);
    }
    REQUIRE(!fake2.has_ref());
    REQUIRE(fake2.called());

    call4.reset(&fake2);
    REQUIRE(fake1.called());
    REQUIRE(!fake1.has_ref());
    call4.reset();
    REQUIRE(!fake2.has_ref());
    REQUIRE(fake2.called());

    call1.reset(&fake1);
    call2.swap(call1);
    REQUIRE((call2 && !call1));

    call2.release();
    REQUIRE(!fake1.called());
    REQUIRE(!fake1.has_ref());
    REQUIRE(!call2);

    REQUIRE(*call1.addressof() == nullptr);

    call1.reset(&fake1);
    fake2.closes = 0;
    fake2.refs = 1;
    *(&call1) = &fake2;
    REQUIRE(!fake1.has_ref());
    REQUIRE(fake1.called());
    REQUIRE(fake2.has_ref());

    call1.reset(&fake1);
    fake2.closes = 0;
    fake2.refs = 1;
    *call1.put() = &fake2;
    REQUIRE(!fake1.has_ref());
    REQUIRE(fake1.called());
    REQUIRE(fake2.has_ref());

    call1.reset();
    REQUIRE(!fake2.has_ref());
    REQUIRE(fake2.called());
}

static bool g_called = false;
static bool called()
{
    auto call = g_called;
    g_called = false;
    return (call);
}

static void __stdcall FakeCall()
{
    g_called = true;
}

using unique_fake_call = wil::unique_call<decltype(&FakeCall), FakeCall>;

TEST_CASE("ResourceTests::VerifyUniqueCall", "[resource][unique_call]")
{
    unique_fake_call call1;
    unique_fake_call call2;

    // intentional compilation errors
    // unique_fake_call call3 = call1;
    // call2 = call1;

    unique_fake_call call4;
    REQUIRE(!called());

    unique_fake_call call5(wistd::move(call4));
    REQUIRE(!call4);
    REQUIRE(call5);

    call4 = wistd::move(call5);
    REQUIRE(call4);
    REQUIRE(!call5);
    REQUIRE(!called());

    {
        unique_fake_call scoped;
    }
    REQUIRE(called());

    call4.reset();
    REQUIRE(called());
    call4.reset();
    REQUIRE(!called());

    call1.release();
    REQUIRE((!call1 && call2));
    call2.swap(call1);
    REQUIRE((call1 && !call2));

    call2.release();
    REQUIRE(!called());
    REQUIRE(!call2);

#ifdef __WIL__ROAPI_H_APPEXCEPTIONAL
    {
        auto call = wil::RoInitialize();
    }
#endif
#ifdef __WIL__ROAPI_H_APP
    {
        wil::unique_rouninitialize_call uninit;
        uninit.release();

        auto call = wil::RoInitialize_failfast();
    }
#endif
#ifdef __WIL__COMBASEAPI_H_APPEXCEPTIONAL
    {
        auto call = wil::CoInitializeEx();
    }
#endif
#ifdef __WIL__COMBASEAPI_H_APP
    {
        wil::unique_couninitialize_call uninit;
        uninit.release();

        auto call = wil::CoInitializeEx_failfast();
    }
#endif
}

// NOLINTNEXTLINE(misc-use-internal-linkage): Compilation only test...
void UniqueCallCompilationTest()
{
#ifdef __WIL__COMBASEAPI_H_EXCEPTIONAL
    {
        auto call = wil::CoImpersonateClient();
    }
#endif
#ifdef __WIL__COMBASEAPI_H_
    {
        wil::unique_coreverttoself_call uninit;
        uninit.release();

        auto call = wil::CoImpersonateClient_failfast();
    }
#endif
}

template <typename StringType, typename VerifyContents>
static void TestStringMaker(VerifyContents&& verifyContents)
{
    PCWSTR values[] = {
        L"",
        L"value",
        // 300 chars
        L"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
        L"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
        L"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"};

    for (const auto& value : values)
    {
        auto const valueLength = wcslen(value);

        // Direct construction case.
        wil::details::string_maker<StringType> maker;
        THROW_IF_FAILED(maker.make(value, valueLength));
        auto result = maker.release();
        verifyContents(value, valueLength, result);

        // Two phase construction case.
        THROW_IF_FAILED(maker.make(nullptr, valueLength));
        REQUIRE(maker.buffer() != nullptr);
        // In the case of the wil::unique_hstring and the empty string the buffer is in a read only
        // section and can't be written to, so StringCchCopy(maker.buffer(), valueLength + 1, value) will fault adding the nul
        // terminator. Use memcpy_s specifying exact size that will be zero in this case instead.
        memcpy_s(maker.buffer(), valueLength * sizeof(*value), value, valueLength * sizeof(*value));
        result = maker.release();
        verifyContents(value, valueLength, result);

        {
            // no promote, ensure no leaks (not tested here, inspect in the debugger)
            wil::details::string_maker<StringType> maker2;
            THROW_IF_FAILED(maker2.make(value, valueLength));
        }
    }
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename StringType>
static void VerifyMakeUniqueString(bool nullValueSupported = true)
{
    if (nullValueSupported)
    {
        auto value0 = wil::make_unique_string<StringType>(nullptr, 5);
    }

    struct
    {
        PCWSTR expectedValue;
        PCWSTR testValue;
        // this is an optional parameter
        size_t testLength = static_cast<size_t>(-1);
    } const testCaseEntries[] = {
        {L"value", L"value", 5},
        {L"value", L"value"},
        {L"va", L"va\0ue", 5},
        {L"v", L"value", 1},
        {L"\0", L"", 5},
        {L"\0", nullptr, 5},
    };

    using maker = wil::details::string_maker<StringType>;
    for (auto const& entry : testCaseEntries)
    {
        bool shouldSkipNullString = ((wcscmp(entry.expectedValue, L"\0") == 0) && !nullValueSupported);
        if (!shouldSkipNullString)
        {
            auto desiredValue = wil::make_unique_string<StringType>(entry.expectedValue);
            auto stringValue = wil::make_unique_string<StringType>(entry.testValue, entry.testLength);
            auto stringValueNoThrow = wil::make_unique_string_nothrow<StringType>(entry.testValue, entry.testLength);
            auto stringValueFailFast = wil::make_unique_string_failfast<StringType>(entry.testValue, entry.testLength);
            REQUIRE(wcscmp(maker::get(desiredValue), maker::get(stringValue)) == 0);
            REQUIRE(wcscmp(maker::get(desiredValue), maker::get(stringValueNoThrow)) == 0);
            REQUIRE(wcscmp(maker::get(desiredValue), maker::get(stringValueFailFast)) == 0);
        }
    }
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerCoTaskMem", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_cotaskmem_string>();
    TestStringMaker<wil::unique_cotaskmem_string>([](PCWSTR value, size_t /*valueLength*/, const wil::unique_cotaskmem_string& result) {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerLocalAlloc", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_hlocal_string>();
    TestStringMaker<wil::unique_hlocal_string>([](PCWSTR value, size_t /*valueLength*/, const wil::unique_hlocal_string& result) {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerGlobalAlloc", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_hglobal_string>();
    TestStringMaker<wil::unique_hglobal_string>([](PCWSTR value, size_t /*valueLength*/, const wil::unique_hglobal_string& result) {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerProcessHeap", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_process_heap_string>();
    TestStringMaker<wil::unique_process_heap_string>([](PCWSTR value, size_t /*valueLength*/, const wil::unique_process_heap_string& result) {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}
#endif

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerMidl", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_midl_string>();
    TestStringMaker<wil::unique_midl_string>([](PCWSTR value, size_t /*valueLength*/, const wil::unique_midl_string& result) {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerHString", "[resource][string_maker]")
{
    wil::unique_hstring value;
    value.reset(static_cast<HSTRING>(nullptr));

    VerifyMakeUniqueString<wil::unique_hstring>(false);

    TestStringMaker<wil::unique_hstring>([](PCWSTR value, size_t valueLength, const wil::unique_hstring& result) {
        UINT32 length;
        REQUIRE(wcscmp(value, WindowsGetStringRawBuffer(result.get(), &length)) == 0);
        REQUIRE(valueLength == length);
    });
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerStdWString", "[resource][string_maker]")
{
    wil::details::string_maker<std::wstring> maker;

    TestStringMaker<std::wstring>([](PCWSTR value, size_t valueLength, const std::wstring& result) {
        REQUIRE(wcscmp(value, result.c_str()) == 0);
        REQUIRE(result == value);
        REQUIRE(result.size() == valueLength);
    });
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyLegacyStringMakers", "[resource][string_maker]")
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    auto localStr = wil::make_hlocal_string(L"value");
    localStr = wil::make_hlocal_string_nothrow(L"value");
    localStr = wil::make_hlocal_string_failfast(L"value");

    auto heapStr = wil::make_process_heap_string(L"value");
    heapStr = wil::make_process_heap_string_nothrow(L"value");
    heapStr = wil::make_process_heap_string_failfast(L"value");
#endif
    auto coTaskStr = wil::make_cotaskmem_string(L"value");
    coTaskStr = wil::make_cotaskmem_string_nothrow(L"value");
    coTaskStr = wil::make_cotaskmem_string_failfast(L"value");
}
#endif

_Use_decl_annotations_
void* __RPC_USER MIDL_user_allocate(size_t size)
{
    return ::HeapAlloc(GetProcessHeap(), 0, size);
}

_Use_decl_annotations_
void __RPC_USER MIDL_user_free(void* ptr)
{
    ::HeapFree(GetProcessHeap(), 0, ptr);
}

TEST_CASE("UniqueMidlStringTests", "[resource][rpc]")
{
    wil::unique_midl_ptr<int[]> intArray{reinterpret_cast<int*>(::MIDL_user_allocate(sizeof(int) * 10))};
    intArray[2] = 1;

    wil::unique_midl_ptr<int> intSingle{reinterpret_cast<int*>(::MIDL_user_allocate(sizeof(int) * 1))};
}

TEST_CASE("UniqueEnvironmentStrings", "[resource][win32]")
{
    wil::unique_environstrings_ptr env{::GetEnvironmentStringsW()};
    const wchar_t* nextVar = env.get();
    while (nextVar && *nextVar)
    {
        // consume 'nextVar'
        nextVar += wcslen(nextVar) + 1;
    }

    wil::unique_environansistrings_ptr envAnsi{::GetEnvironmentStringsA()};
    const char* nextVarAnsi = envAnsi.get();
    while (nextVarAnsi && *nextVarAnsi)
    {
        // consume 'nextVar'
        nextVarAnsi += strlen(nextVarAnsi) + 1;
    }
}

#if (__STDC__ && !defined(_FORCENAMELESSUNION)) || defined(NONAMELESSUNION) || \
    (!defined(_MSC_EXTENSIONS) && !defined(_FORCENAMELESSUNION))
#define VAR_ACCESS_1 .n1
#define VAR_ACCESS_2 .n1.n2
#define VAR_ACCESS_3 .n1.n2.n3
#define VAR_ACCESS_4 .n1.n2.n3.brecVal
#else
#define VAR_ACCESS_1
#define VAR_ACCESS_2
#define VAR_ACCESS_3
#define VAR_ACCESS_4
#endif

TEST_CASE("UniqueVariant", "[resource][com]")
{
    wil::unique_variant var;
    var VAR_ACCESS_2.vt = VT_BSTR;
    var VAR_ACCESS_3.bstrVal = ::SysAllocString(L"25");
    REQUIRE(var VAR_ACCESS_3.bstrVal != nullptr);

    auto call = [](const VARIANT&) {};
    call(var);

    VARIANT weakVar = var;
    (void)weakVar;

    wil::unique_variant var2;
    REQUIRE_SUCCEEDED(VariantChangeType(&var2, &var, 0, VT_UI4));
    REQUIRE(var2 VAR_ACCESS_2.vt == VT_UI4);
    REQUIRE(var2 VAR_ACCESS_3.uiVal == 25);
}

TEST_CASE("DefaultTemplateParamCompiles", "[resource]")
{
    wil::unique_process_heap_ptr<> heapPtr;
    wil::unique_virtualalloc_ptr<> virtualPtr;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    wil::unique_hlocal_ptr<> hlocalPtr;
    wil::unique_hlocal_secure_ptr<> hlocalSecurePtr;
    wil::unique_hglobal_ptr<> hglobalPtr;
    wil::unique_cotaskmem_secure_ptr<> cotaskmemSecurePtr;
#endif

    wil::unique_midl_ptr<> midlPtr;
    wil::unique_cotaskmem_ptr<> cotaskmemPtr;
    wil::unique_mapview_ptr<> mapViewPtr;
}

TEST_CASE("UniqueInvokeCleanupMembers", "[resource]")
{
    // Case 1 - unique_ptr<> for a T* that has a "destroy" member
    struct ThingWithDestroy
    {
        bool destroyed = false;
        void destroy()
        {
            destroyed = true;
        };
    };
    ThingWithDestroy toDestroy;
    wil::unique_any<ThingWithDestroy*, decltype(&ThingWithDestroy::destroy), &ThingWithDestroy::destroy> obj(&toDestroy);
    obj.reset();
    REQUIRE(!obj);
    REQUIRE(toDestroy.destroyed);

    // Case 2 - unique_struct calling a member, like above
    struct ThingToDestroy2
    {
        bool* destroyed;
        void destroy() const
        {
            *destroyed = true;
        };
    };
    bool structDestroyed = false;
    {
        wil::unique_struct<ThingToDestroy2, decltype(&ThingToDestroy2::destroy), &ThingToDestroy2::destroy> other;
        other.destroyed = &structDestroyed;
        REQUIRE(!structDestroyed);
    }
    REQUIRE(structDestroyed);
}

struct ITokenTester : IUnknown
{
    virtual void DirectClose(DWORD_PTR token) = 0;
};

struct TokenTester : ITokenTester
{
    IFACEMETHOD_(ULONG, AddRef)() override
    {
        return 2;
    }
    IFACEMETHOD_(ULONG, Release)() override
    {
        return 1;
    }
    IFACEMETHOD(QueryInterface)(REFIID, void**)
    {
        return E_NOINTERFACE;
    }
    void DirectClose(DWORD_PTR token) override
    {
        m_closed = (token == m_closeToken);
    }
    bool m_closed = false;
    DWORD_PTR m_closeToken;
};

static void MyTokenTesterCloser(ITokenTester* obj, DWORD_PTR token)
{
    obj->DirectClose(token);
}

TEST_CASE("ComTokenCloser", "[resource]")
{
    using token_tester_t = wil::unique_com_token<ITokenTester, DWORD_PTR, decltype(MyTokenTesterCloser), &MyTokenTesterCloser>;

    TokenTester obj;
    obj.m_closeToken = 4;
    {
        token_tester_t tmp{&obj, 4};
    }
    REQUIRE(obj.m_closed);
}

TEST_CASE("ComTokenDirectCloser", "[resource]")
{
    using token_tester_t =
        wil::unique_com_token<ITokenTester, DWORD_PTR, decltype(&ITokenTester::DirectClose), &ITokenTester::DirectClose>;

    TokenTester obj;
    obj.m_closeToken = 4;
    {
        token_tester_t tmp{&obj, 4};
    }
    REQUIRE(obj.m_closed);
}

TEST_CASE("UniqueCloseClipboardCall", "[resource]")
{
#if defined(__WIL__WINUSER_) && !defined(NOCLIPBOARD)
    if (auto clip = wil::open_clipboard(nullptr))
    {
        REQUIRE(::EmptyClipboard());
    }
#endif
}

void read_lock_function([[maybe_unused]] wil::read_lock_required lock)
{
}

void write_lock_function([[maybe_unused]] wil::write_lock_required lock)
{
}

TEST_CASE("read_lock_required", "[resource]")
{
    wil::srwlock lock;
    read_lock_function(lock.lock_shared());
    read_lock_function(lock.lock_exclusive()); // an exclusive lock also counts as a read lock

    wil::critical_section cs;
    read_lock_function(cs.lock());

    std::recursive_mutex mutex;
    read_lock_function(std::lock_guard<std::recursive_mutex>(mutex));
    read_lock_function(std::scoped_lock(mutex));

    std::shared_mutex sharedMutex;
    read_lock_function(std::shared_lock<std::shared_mutex>(sharedMutex));
    read_lock_function(std::unique_lock<std::shared_mutex>(sharedMutex));
}

TEST_CASE("write_lock_required", "[resource]")
{
    wil::srwlock lock;
    write_lock_function(lock.lock_exclusive());

    wil::critical_section cs;
    write_lock_function(cs.lock());

    std::recursive_mutex mutex;
    write_lock_function(std::lock_guard<std::recursive_mutex>(mutex));
    write_lock_function(std::scoped_lock(mutex));

    std::shared_mutex sharedMutex;
    write_lock_function(std::unique_lock<std::shared_mutex>(sharedMutex));
}

// ---------------------------------------------------------------------------
// Experimental "better" wil::format (std::format-like) prototype.
//
// Goals:
//   * std::format-like call syntax with variadic templates (compile-time arg
//     type validation) rather than printf-style va_args.
//   * Avoid the binary bloat of fully statically-compiled formatters by
//     type-erasing each argument behind a tiny stack-allocated callback that
//     is asked (via a virtual call) to render itself into an output sink. The
//     format engine itself is compiled exactly once regardless of the argument
//     types in play.
//   * NO dependency on <format>/<fmt> and NO exceptions: the core is "nothrow"
//     and reports failures via HRESULT, so it is usable from C++20 code that
//     does not want either. Built-in types are formatted by small hand-written
//     routines; a practical subset of the std::format spec mini-language is
//     supported.
//   * Allow custom "format wrappers" that provide their own size/format logic.
//
// This is being prototyped here in ResourceTests.cpp and will be ported into
// wil/resource.h once the shape is settled.
// ---------------------------------------------------------------------------
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace wil_experimental
{
    // The destination that format pieces are written into. There are two
    // concrete sinks: one that only measures (counts characters) and one that
    // writes into a pre-sized buffer. The whole format operation runs twice:
    // once to measure, once to write. This mirrors wil::str_build_nothrow and
    // avoids reallocation / temporary string churn. Sinks never fail.
    struct format_sink
    {
        virtual void append(const wchar_t* data, size_t count) noexcept = 0;

        void append(std::wstring_view text) noexcept
        {
            append(text.data(), text.size());
        }

        void append(wchar_t c) noexcept
        {
            append(&c, 1);
        }

    protected:
        ~format_sink() = default;
    };

    // Counts characters without storing them.
    struct measuring_sink final : format_sink
    {
        size_t count = 0;

        void append(const wchar_t*, size_t n) noexcept override
        {
            count += n;
        }
    };

    // Writes characters into a fixed-size buffer (never overruns it).
    struct buffer_sink final : format_sink
    {
        wchar_t* cursor = nullptr;
        wchar_t* end = nullptr;

        void append(const wchar_t* data, size_t n) noexcept override
        {
            const size_t remaining = static_cast<size_t>(end - cursor);
            const size_t toCopy = n < remaining ? n : remaining;
            ::memcpy(cursor, data, toCopy * sizeof(wchar_t));
            cursor += toCopy;
        }
    };

    // Parsed form of a std::format-style replacement-field spec. Supported:
    //   [[fill]align][sign]["#"]["0"][width]["." precision][type]
    // Not supported (yet): nested replacement fields for dynamic width or
    // precision (e.g. "{:{}}"), and locale ('L').
    struct format_spec
    {
        wchar_t fill = L' ';
        wchar_t align = 0;    // 0 (unset), '<', '>', '^', or '=' (after sign)
        wchar_t sign = L'-';  // '-' (negatives only), '+', or ' '
        bool alternate = false; // '#'
        bool zero = false;      // '0'
        int width = 0;
        int precision = -1; // -1 == unset
        wchar_t type = 0;   // presentation type, 0 == default
    };

    constexpr bool is_align_char(wchar_t c) noexcept
    {
        return c == L'<' || c == L'>' || c == L'^';
    }

    // Parse a spec string. Returns E_INVALIDARG for malformed specs.
    inline HRESULT parse_format_spec(std::wstring_view spec, format_spec& out) noexcept
    {
        out = format_spec{};
        size_t pos = 0;
        const size_t n = spec.size();

        // [[fill]align]
        if (n - pos >= 2 && is_align_char(spec[pos + 1]))
        {
            out.fill = spec[pos];
            out.align = spec[pos + 1];
            pos += 2;
        }
        else if (pos < n && is_align_char(spec[pos]))
        {
            out.align = spec[pos];
            ++pos;
        }

        // [sign]
        if (pos < n && (spec[pos] == L'+' || spec[pos] == L'-' || spec[pos] == L' '))
        {
            out.sign = spec[pos];
            ++pos;
        }

        // ["#"]
        if (pos < n && spec[pos] == L'#')
        {
            out.alternate = true;
            ++pos;
        }

        // ["0"] - zero padding. Ignored if an explicit alignment was supplied.
        if (pos < n && spec[pos] == L'0')
        {
            out.zero = true;
            ++pos;
        }

        // [width]
        if (pos < n && spec[pos] >= L'1' && spec[pos] <= L'9')
        {
            int width = 0;
            while (pos < n && spec[pos] >= L'0' && spec[pos] <= L'9')
            {
                width = (width * 10) + (spec[pos] - L'0');
                ++pos;
            }
            out.width = width;
        }

        // ["." precision]
        if (pos < n && spec[pos] == L'.')
        {
            ++pos;
            RETURN_HR_IF(E_INVALIDARG, pos >= n || spec[pos] < L'0' || spec[pos] > L'9');
            int precision = 0;
            while (pos < n && spec[pos] >= L'0' && spec[pos] <= L'9')
            {
                precision = (precision * 10) + (spec[pos] - L'0');
                ++pos;
            }
            out.precision = precision;
        }

        // [type]
        if (pos < n)
        {
            out.type = spec[pos];
            ++pos;
        }

        RETURN_HR_IF(E_INVALIDARG, pos != n); // trailing garbage
        return S_OK;
    }

    // Append 'count' copies of 'ch' to the sink.
    inline void append_fill(format_sink& sink, wchar_t ch, size_t count) noexcept
    {
        wchar_t chunk[32];
        for (auto& c : chunk)
        {
            c = ch;
        }
        while (count > 0)
        {
            const size_t batch = count < 32 ? count : 32;
            sink.append(chunk, batch);
            count -= batch;
        }
    }

    // Emit 'body' padded to 'width'. 'zeroPoint' is the index in body after any
    // sign/prefix, where zero-padding is inserted for '=' alignment.
    inline void emit_aligned(
        format_sink& sink, const wchar_t* body, size_t bodyLen, size_t zeroPoint, const format_spec& ps, wchar_t defaultAlign) noexcept
    {
        const size_t width = ps.width > 0 ? static_cast<size_t>(ps.width) : 0;
        if (bodyLen >= width)
        {
            sink.append(body, bodyLen);
            return;
        }

        wchar_t align = ps.align;
        wchar_t fill = ps.fill;
        if (ps.zero && ps.align == 0)
        {
            align = L'=';
            fill = L'0';
        }
        if (align == 0)
        {
            align = defaultAlign;
        }

        const size_t pad = width - bodyLen;
        switch (align)
        {
        case L'<':
            sink.append(body, bodyLen);
            append_fill(sink, fill, pad);
            break;
        case L'^':
        {
            const size_t left = pad / 2;
            append_fill(sink, fill, left);
            sink.append(body, bodyLen);
            append_fill(sink, fill, pad - left);
            break;
        }
        case L'=':
            sink.append(body, zeroPoint);
            append_fill(sink, fill, pad);
            sink.append(body + zeroPoint, bodyLen - zeroPoint);
            break;
        case L'>':
        default:
            append_fill(sink, fill, pad);
            sink.append(body, bodyLen);
            break;
        }
    }

    // Convert an unsigned magnitude to digits in the given base. Writes into
    // 'buffer' (must hold at least 64 wchar_t) and returns the count.
    inline size_t magnitude_to_digits(unsigned long long mag, unsigned base, bool upper, wchar_t* buffer) noexcept
    {
        const wchar_t* digits = upper ? L"0123456789ABCDEF" : L"0123456789abcdef";
        wchar_t temp[64];
        size_t count = 0;
        do
        {
            temp[count++] = digits[mag % base];
            mag /= base;
        } while (mag != 0);

        for (size_t i = 0; i < count; ++i)
        {
            buffer[i] = temp[count - 1 - i];
        }
        return count;
    }

    inline HRESULT format_integer(format_sink& sink, unsigned long long mag, bool negative, const format_spec& ps) noexcept
    {
        unsigned base = 10;
        bool upper = false;
        const wchar_t* prefix = L"";
        switch (ps.type)
        {
        case 0:
        case L'd':
            base = 10;
            break;
        case L'x':
            base = 16;
            prefix = L"0x";
            break;
        case L'X':
            base = 16;
            upper = true;
            prefix = L"0X";
            break;
        case L'o':
            base = 8;
            prefix = L"0";
            break;
        case L'b':
            base = 2;
            prefix = L"0b";
            break;
        case L'B':
            base = 2;
            prefix = L"0B";
            break;
        default:
            return E_INVALIDARG;
        }

        wchar_t body[80];
        size_t len = 0;

        // Sign.
        if (negative)
        {
            body[len++] = L'-';
        }
        else if (ps.sign == L'+')
        {
            body[len++] = L'+';
        }
        else if (ps.sign == L' ')
        {
            body[len++] = L' ';
        }

        // Alternate-form prefix.
        if (ps.alternate && base != 10)
        {
            for (const wchar_t* p = prefix; *p; ++p)
            {
                body[len++] = *p;
            }
        }

        const size_t zeroPoint = len;
        len += magnitude_to_digits(mag, base, upper, body + len);

        emit_aligned(sink, body, len, zeroPoint, ps, L'>');
        return S_OK;
    }

    inline HRESULT format_pointer(format_sink& sink, const void* value, const format_spec& ps) noexcept
    {
        wchar_t body[2 + 16 + 1];
        size_t len = 0;
        body[len++] = L'0';
        body[len++] = L'x';
        const size_t zeroPoint = len;
        len += magnitude_to_digits(reinterpret_cast<uintptr_t>(value), 16, false, body + len);
        emit_aligned(sink, body, len, zeroPoint, ps, L'>');
        return S_OK;
    }

    inline HRESULT format_floating(format_sink& sink, long double value, const format_spec& ps) noexcept
    {
        // Build a narrow printf-style format string and let the CRT do the
        // numeric conversion (one shared routine, no per-type bloat). Width and
        // alignment are then applied by emit_aligned so std::format fill/align
        // semantics are honored.
        wchar_t pf[16];
        size_t k = 0;
        pf[k++] = L'%';
        if (ps.sign == L'+')
        {
            pf[k++] = L'+';
        }
        else if (ps.sign == L' ')
        {
            pf[k++] = L' ';
        }
        if (ps.alternate)
        {
            pf[k++] = L'#';
        }

        wchar_t type = ps.type;
        if (type == 0)
        {
            type = L'g';
        }
        switch (type)
        {
        case L'f':
        case L'F':
        case L'e':
        case L'E':
        case L'g':
        case L'G':
        case L'a':
        case L'A':
            break;
        default:
            return E_INVALIDARG;
        }

        if (ps.precision >= 0)
        {
            pf[k++] = L'.';
            pf[k++] = L'*';
        }
        pf[k++] = L'L';
        pf[k++] = type;
        pf[k] = L'\0';

        wchar_t buffer[512];
        if (ps.precision >= 0)
        {
            RETURN_IF_FAILED(StringCchPrintfW(buffer, ARRAYSIZE(buffer), pf, ps.precision, value));
        }
        else
        {
            RETURN_IF_FAILED(StringCchPrintfW(buffer, ARRAYSIZE(buffer), pf, value));
        }

        const size_t len = wcslen(buffer);
        const size_t zeroPoint = (len > 0 && (buffer[0] == L'-' || buffer[0] == L'+' || buffer[0] == L' ')) ? 1 : 0;
        emit_aligned(sink, buffer, len, zeroPoint, ps, L'>');
        return S_OK;
    }

    inline HRESULT format_text(format_sink& sink, std::wstring_view text, const format_spec& ps) noexcept
    {
        RETURN_HR_IF(E_INVALIDARG, ps.type != 0 && ps.type != L's');
        if (ps.precision >= 0 && text.size() > static_cast<size_t>(ps.precision))
        {
            text = text.substr(0, static_cast<size_t>(ps.precision));
        }
        emit_aligned(sink, text.data(), text.size(), 0, ps, L'<');
        return S_OK;
    }

    // Base for user-defined format wrappers (the "stretch" goal). Derive from
    // this and provide:
    //     HRESULT format(wil_experimental::format_sink& sink, std::wstring_view spec) const noexcept;
    // The wrapper typically captures its subject by const reference.
    struct custom_format_wrapper
    {
    };

    template <typename T>
    inline constexpr bool is_custom_format_wrapper_v = std::is_base_of_v<custom_format_wrapper, std::remove_cv_t<std::remove_reference_t<T>>>;

    // Detects types that expose a contiguous wchar_t range via .data()/.size()
    // (e.g. std::wstring, std::wstring_view).
    template <typename T, typename = void>
    struct is_wchar_view : std::false_type
    {
    };
    template <typename T>
    struct is_wchar_view<
        T,
        std::void_t<
            decltype(static_cast<const wchar_t*>(std::declval<const T&>().data())),
            decltype(static_cast<size_t>(std::declval<const T&>().size()))>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_wstringish_v = std::is_convertible_v<T, const wchar_t*> || is_wchar_view<T>::value;

    // Type-erased argument interface. Each argument passed to format() is
    // wrapped in a typed_format_argument (allocated on the caller's stack) and
    // referenced here through the base so the format engine itself is compiled
    // exactly once, regardless of the argument types in play.
    struct format_argument
    {
        virtual HRESULT render(format_sink& sink, std::wstring_view spec) const noexcept = 0;

    protected:
        ~format_argument() = default;
    };

    template <typename T>
    struct typed_format_argument final : format_argument
    {
        const T& value;

        explicit typed_format_argument(const T& v) noexcept : value(v)
        {
        }

        HRESULT render(format_sink& sink, std::wstring_view spec) const noexcept override
        {
            if constexpr (is_custom_format_wrapper_v<T>)
            {
                // Custom wrappers do their own spec handling.
                return value.format(sink, spec);
            }
            else
            {
                format_spec ps;
                RETURN_IF_FAILED(parse_format_spec(spec, ps));

                if constexpr (is_wstringish_v<T>)
                {
                    return format_text(sink, to_view(value), ps);
                }
                else if constexpr (std::is_same_v<T, wchar_t> || std::is_same_v<T, char>)
                {
                    if (ps.type == 0 || ps.type == L'c')
                    {
                        const wchar_t c = static_cast<wchar_t>(value);
                        emit_aligned(sink, &c, 1, 0, ps, L'<');
                        return S_OK;
                    }
                    return format_integer(sink, static_cast<unsigned long long>(static_cast<unsigned char>(value)), false, ps);
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    if (ps.type == 0 || ps.type == L's')
                    {
                        return format_text(sink, value ? std::wstring_view(L"true") : std::wstring_view(L"false"), ps);
                    }
                    return format_integer(sink, value ? 1u : 0u, false, ps);
                }
                else if constexpr (std::is_integral_v<T>)
                {
                    if constexpr (std::is_signed_v<T>)
                    {
                        if (value < 0)
                        {
                            const auto mag = static_cast<unsigned long long>(0) - static_cast<unsigned long long>(value);
                            return format_integer(sink, mag, true, ps);
                        }
                    }
                    return format_integer(sink, static_cast<unsigned long long>(value), false, ps);
                }
                else if constexpr (std::is_floating_point_v<T>)
                {
                    return format_floating(sink, static_cast<long double>(value), ps);
                }
                else if constexpr (std::is_pointer_v<T>)
                {
                    return format_pointer(sink, static_cast<const void*>(value), ps);
                }
                else
                {
                    static_assert(sizeof(T) == 0, "wil_experimental::format does not support this argument type");
                    return E_NOTIMPL;
                }
            }
        }

    private:
        template <typename U>
        static std::wstring_view to_view(const U& v) noexcept
        {
            if constexpr (std::is_convertible_v<U, const wchar_t*>)
            {
                const wchar_t* p = v;
                return p ? std::wstring_view(p) : std::wstring_view();
            }
            else
            {
                return std::wstring_view(v.data(), v.size());
            }
        }
    };

    // The single, non-templated format engine. Splits the format string into
    // literal runs and replacement fields and routes each field to the right
    // type-erased argument. Nothrow: reports malformed input via HRESULT.
    //
    // Supported: auto-indexing ({}), manual indexing ({0}), escaped braces
    // ({{ and }}), and the spec subset documented on format_spec. Not yet
    // supported: nested replacement fields for dynamic width/precision.
    inline HRESULT run_format(
        format_sink& sink, std::wstring_view fmt, const format_argument* const* args, size_t argCount) noexcept
    {
        size_t autoIndex = 0;
        size_t pos = 0;
        const size_t n = fmt.size();

        while (pos < n)
        {
            // Emit the run of literal text up to the next brace.
            const size_t literalStart = pos;
            while (pos < n && fmt[pos] != L'{' && fmt[pos] != L'}')
            {
                ++pos;
            }
            if (pos > literalStart)
            {
                sink.append(fmt.data() + literalStart, pos - literalStart);
            }
            if (pos >= n)
            {
                break;
            }

            const wchar_t brace = fmt[pos];
            if ((pos + 1 < n) && (fmt[pos + 1] == brace))
            {
                sink.append(brace);
                pos += 2;
                continue;
            }

            RETURN_HR_IF(E_INVALIDARG, brace == L'}'); // stray '}'

            // Parse a replacement field: '{' [index] [':' spec] '}'
            ++pos; // consume '{'

            size_t index = 0;
            bool hasIndex = false;
            while (pos < n && fmt[pos] >= L'0' && fmt[pos] <= L'9')
            {
                index = (index * 10) + static_cast<size_t>(fmt[pos] - L'0');
                hasIndex = true;
                ++pos;
            }
            if (!hasIndex)
            {
                index = autoIndex++;
            }

            std::wstring_view spec;
            if (pos < n && fmt[pos] == L':')
            {
                ++pos; // consume ':'
                const size_t specStart = pos;
                while (pos < n && fmt[pos] != L'}')
                {
                    ++pos;
                }
                spec = fmt.substr(specStart, pos - specStart);
            }

            RETURN_HR_IF(E_INVALIDARG, pos >= n || fmt[pos] != L'}'); // unterminated field
            ++pos;                                                    // consume '}'

            RETURN_HR_IF(E_INVALIDARG, index >= argCount);
            RETURN_IF_FAILED(args[index]->render(sink, spec));
        }
        return S_OK;
    }

    // Constexpr spec parser (a no-WIL-macros twin of parse_format_spec) so it
    // can run during constant evaluation. Returns false on malformed specs.
    constexpr bool parse_spec_ct(std::wstring_view spec, format_spec& out) noexcept
    {
        out = format_spec{};
        size_t pos = 0;
        const size_t n = spec.size();

        if (n - pos >= 2 && is_align_char(spec[pos + 1]))
        {
            out.fill = spec[pos];
            out.align = spec[pos + 1];
            pos += 2;
        }
        else if (pos < n && is_align_char(spec[pos]))
        {
            out.align = spec[pos];
            ++pos;
        }

        if (pos < n && (spec[pos] == L'+' || spec[pos] == L'-' || spec[pos] == L' '))
        {
            out.sign = spec[pos];
            ++pos;
        }
        if (pos < n && spec[pos] == L'#')
        {
            out.alternate = true;
            ++pos;
        }
        if (pos < n && spec[pos] == L'0')
        {
            out.zero = true;
            ++pos;
        }
        if (pos < n && spec[pos] >= L'1' && spec[pos] <= L'9')
        {
            int width = 0;
            while (pos < n && spec[pos] >= L'0' && spec[pos] <= L'9')
            {
                width = (width * 10) + (spec[pos] - L'0');
                ++pos;
            }
            out.width = width;
        }
        if (pos < n && spec[pos] == L'.')
        {
            ++pos;
            if (pos >= n || spec[pos] < L'0' || spec[pos] > L'9')
            {
                return false;
            }
            int precision = 0;
            while (pos < n && spec[pos] >= L'0' && spec[pos] <= L'9')
            {
                precision = (precision * 10) + (spec[pos] - L'0');
                ++pos;
            }
            out.precision = precision;
        }
        if (pos < n)
        {
            out.type = spec[pos];
            ++pos;
        }
        return pos == n;
    }

    // Compile-time validation of a single field's spec against the static type
    // of the argument it formats. This mirrors what the runtime per-type
    // formatters accept (and what std::formatter<T> would allow): the
    // presentation type must be sensible for T, and options like precision or
    // sign are only permitted where they make sense. Custom format wrappers opt
    // out (they validate their own spec at runtime).
    template <typename T>
    constexpr bool validate_spec_for(std::wstring_view spec) noexcept
    {
        if constexpr (is_custom_format_wrapper_v<T>)
        {
            return true;
        }
        else
        {
            format_spec ps;
            if (!parse_spec_ct(spec, ps))
            {
                return false;
            }
            const wchar_t t = ps.type;
            const bool hasPrecision = ps.precision >= 0;
            const bool hasNumericFlags = (ps.sign != L'-') || ps.alternate || ps.zero;

            if constexpr (is_wstringish_v<T>)
            {
                // strings: only 's', no sign/#/0; precision (truncation) allowed.
                return (t == 0 || t == L's') && !hasNumericFlags;
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                return (t == 0 || t == L's' || t == L'd' || t == L'b' || t == L'B' || t == L'o' || t == L'x' || t == L'X') &&
                       !hasPrecision;
            }
            else if constexpr (std::is_same_v<T, wchar_t> || std::is_same_v<T, char>)
            {
                return (t == 0 || t == L'c' || t == L'd' || t == L'b' || t == L'B' || t == L'o' || t == L'x' || t == L'X') &&
                       !hasPrecision;
            }
            else if constexpr (std::is_integral_v<T>)
            {
                return (t == 0 || t == L'd' || t == L'b' || t == L'B' || t == L'o' || t == L'x' || t == L'X') && !hasPrecision;
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                return (t == 0 || t == L'a' || t == L'A' || t == L'e' || t == L'E' || t == L'f' || t == L'F' || t == L'g' ||
                        t == L'G');
            }
            else if constexpr (std::is_pointer_v<T>)
            {
                return (t == 0 || t == L'p' || t == L'P') && !hasNumericFlags && !hasPrecision;
            }
            else
            {
                return false; // unsupported argument type
            }
        }
    }

    using spec_validator_fn = bool (*)(std::wstring_view);

    // Core constexpr validator. Walks the format string checking brace balance,
    // field syntax, index range, and auto/manual indexing rules. When
    // 'validators' is non-null it additionally type-checks each field's spec
    // against the argument at that index (validators must then have one entry
    // per argument).
    constexpr bool validate_impl(std::wstring_view fmt, size_t argCount, const spec_validator_fn* validators) noexcept
    {
        size_t pos = 0;
        const size_t n = fmt.size();
        size_t autoIndex = 0;
        bool usedAuto = false;
        bool usedManual = false;

        while (pos < n)
        {
            const wchar_t c = fmt[pos];
            if (c == L'{')
            {
                if (pos + 1 < n && fmt[pos + 1] == L'{')
                {
                    pos += 2; // escaped '{{'
                    continue;
                }

                ++pos; // consume '{'

                bool hasIndex = false;
                size_t index = 0;
                while (pos < n && fmt[pos] >= L'0' && fmt[pos] <= L'9')
                {
                    index = (index * 10) + static_cast<size_t>(fmt[pos] - L'0');
                    hasIndex = true;
                    ++pos;
                }

                size_t fieldIndex = 0;
                if (hasIndex)
                {
                    usedManual = true;
                    if (index >= argCount)
                    {
                        return false; // index out of range
                    }
                    fieldIndex = index;
                }
                else
                {
                    usedAuto = true;
                    if (autoIndex >= argCount)
                    {
                        return false; // more {} fields than arguments
                    }
                    fieldIndex = autoIndex;
                    ++autoIndex;
                }

                std::wstring_view spec;
                if (pos < n && fmt[pos] == L':')
                {
                    ++pos; // consume ':'
                    const size_t specStart = pos;
                    while (pos < n && fmt[pos] != L'}')
                    {
                        ++pos;
                    }
                    spec = fmt.substr(specStart, pos - specStart);
                }

                if (pos >= n || fmt[pos] != L'}')
                {
                    return false; // unterminated replacement field
                }
                ++pos; // consume '}'

                if (validators && !validators[fieldIndex](spec))
                {
                    return false; // spec not valid for the argument's type
                }
            }
            else if (c == L'}')
            {
                if (pos + 1 < n && fmt[pos + 1] == L'}')
                {
                    pos += 2; // escaped '}}'
                    continue;
                }
                return false; // stray '}'
            }
            else
            {
                ++pos;
            }
        }

        if (usedAuto && usedManual)
        {
            return false; // cannot mix automatic and manual indexing
        }
        return true;
    }

    // Constexpr structural validator (counts only). Checks that braces are
    // balanced/escaped, that every replacement field is well-formed, that
    // referenced argument indices are in range, that automatic indexing does
    // not run past 'argCount', and that automatic and manual indexing are not
    // mixed. It does NOT type-check the spec against argument types - use
    // validate_types for that. Being constexpr, usable in static_assert.
    constexpr bool validate(std::wstring_view fmt, size_t argCount) noexcept
    {
        return validate_impl(fmt, argCount, nullptr);
    }

    // Constexpr validator that additionally checks each field's spec against the
    // static type of the corresponding argument (Args in order). This is what
    // the consteval format_string constructor uses, so an unsuitable spec such
    // as format(L"{:x}", L"text") or format(L"{:.2f}", 42) fails to compile.
    template <typename... Args>
    constexpr bool validate_types(std::wstring_view fmt) noexcept
    {
        const spec_validator_fn validators[sizeof...(Args) + 1] = {&validate_spec_for<Args>..., nullptr};
        return validate_impl(fmt, sizeof...(Args), validators);
    }

    // Measures, allocates via wil's string_maker, then writes. This is templated
    // only on the output string type (not on the argument types), so the bulk of
    // the formatting work is compiled once per string_type rather than once per
    // unique set of argument types.
    template <typename string_type>
    HRESULT format_to_maker(
        string_type& result, std::wstring_view fmt, const format_argument* const* table, size_t argCount) noexcept
    {
        measuring_sink measure;
        RETURN_IF_FAILED(run_format(measure, fmt, table, argCount));

        wil::details::string_maker<string_type> maker;
        RETURN_IF_FAILED(maker.make(nullptr, measure.count));

        buffer_sink writer;
        writer.cursor = maker.buffer();
        writer.end = writer.cursor + measure.count;
        RETURN_IF_FAILED(run_format(writer, fmt, table, argCount));

        result = maker.release();
        return S_OK;
    }

    // Runtime entry point (the format string is not validated at compile time -
    // analogous to std::vformat). Builds the type-erased argument table on the
    // stack, then hands off to the string-type-only format_to_maker.
    template <typename string_type, typename... Args>
    HRESULT vformat_nothrow(string_type& result, std::wstring_view fmt, const Args&... args) noexcept
    {
        std::tuple<typed_format_argument<Args>...> typedArgs{typed_format_argument<Args>(args)...};

        const format_argument* table[sizeof...(Args) + 1] = {};
        size_t i = 0;
        std::apply(
            [&](auto&... a) {
                ((table[i++] = &a), ...);
            },
            typedArgs);

        return format_to_maker(result, fmt, table, sizeof...(Args));
    }

#ifdef __cpp_consteval
    /// @cond
    namespace details
    {
        // Referencing this non-constexpr function from a constant-evaluated
        // context makes the program ill-formed, which is how an invalid format
        // string is turned into a compile error (no exceptions required).
        inline void invalid_format_string_detected() noexcept
        {
        }
    } // namespace details
    /// @endcond

    // A format-string wrapper whose consteval constructor validates the string
    // against the number of supplied arguments at compile time (modeled on
    // std::basic_format_string). Construction from a non-constant string is
    // itself a compile error, so this is only reachable with literal/constexpr
    // formats.
    template <typename... Args>
    class basic_format_string
    {
    public:
        template <typename T, std::enable_if_t<std::is_convertible_v<const T&, std::wstring_view>, int> = 0>
        consteval basic_format_string(const T& fmt) : m_value(fmt)
        {
            if (!validate_types<Args...>(m_value))
            {
                details::invalid_format_string_detected(); // ill-formed: invalid format string
            }
        }

        WI_NODISCARD constexpr std::wstring_view get() const noexcept
        {
            return m_value;
        }

    private:
        std::wstring_view m_value;
    };

    // Hiding type_identity_t inside the alias (as the standard library does)
    // keeps the argument types a non-deduced context in the signatures below,
    // so Args is deduced solely from the trailing function arguments.
    template <typename... Args>
    using format_string = basic_format_string<std::type_identity_t<Args>...>;

    // Compile-time-validated nothrow entry point. The format string is checked
    // against the argument count (and structure) by basic_format_string's
    // consteval constructor before this is ever called.
    template <typename string_type, typename... Args>
    HRESULT format_nothrow(string_type& result, format_string<Args...> fmt, const Args&... args) noexcept
    {
        return vformat_nothrow(result, fmt.get(), args...);
    }
#else  // !__cpp_consteval
    // Pre-C++20 fallback: no compile-time validation, runtime behavior only.
    template <typename string_type, typename... Args>
    HRESULT format_nothrow(string_type& result, std::wstring_view fmt, const Args&... args) noexcept
    {
        return vformat_nothrow(result, fmt, args...);
    }
#endif // __cpp_consteval

#ifdef WIL_ENABLE_EXCEPTIONS
    // Throwing convenience wrappers, layered on top of the nothrow core. When
    // compile-time validation is available these also reject invalid format
    // strings at compile time.
#ifdef __cpp_consteval
    template <typename string_type, typename... Args>
    string_type format_as(format_string<Args...> fmt, const Args&... args)
    {
        string_type result{};
        THROW_IF_FAILED(vformat_nothrow(result, fmt.get(), args...));
        return result;
    }

    template <typename... Args>
    std::wstring format(format_string<Args...> fmt, const Args&... args)
    {
        return format_as<std::wstring>(fmt, args...);
    }
#else  // !__cpp_consteval
    template <typename string_type, typename... Args>
    string_type format_as(std::wstring_view fmt, const Args&... args)
    {
        string_type result{};
        THROW_IF_FAILED(vformat_nothrow(result, fmt, args...));
        return result;
    }

    template <typename... Args>
    std::wstring format(std::wstring_view fmt, const Args&... args)
    {
        return format_as<std::wstring>(fmt, args...);
    }
#endif // __cpp_consteval
#endif // WIL_ENABLE_EXCEPTIONS

    // Example custom format wrapper used by the tests below: renders an
    // unsigned value as fixed-width hex, regardless of spec.
    struct as_hex8 : custom_format_wrapper
    {
        const unsigned int& value;

        explicit as_hex8(const unsigned int& v) noexcept : value(v)
        {
        }

        HRESULT format(format_sink& sink, std::wstring_view /*spec*/) const noexcept
        {
            wchar_t buffer[2 + 8 + 1];
            RETURN_IF_FAILED(StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"0x%08X", value));
            sink.append(buffer, wcslen(buffer));
            return S_OK;
        }
    };
} // namespace wil_experimental

namespace
{
    // Helper that exercises the nothrow core with a string_maker target that is
    // available even when exceptions are disabled (unlike string_maker<std::wstring>,
    // which lives in wil/stl.h behind WIL_ENABLE_EXCEPTIONS). Returns the formatted
    // text for easy comparison. Uses the runtime (vformat) path because the format
    // string arrives as a runtime parameter here.
    template <typename... Args>
    std::wstring wfmt(std::wstring_view fmt, const Args&... args)
    {
        wil::unique_cotaskmem_string result;
        REQUIRE(SUCCEEDED(wil_experimental::vformat_nothrow(result, fmt, args...)));
        return std::wstring(result.get());
    }
}

TEST_CASE("WilFormat::Basic", "[resource][format]")
{
    SECTION("literals and escaping")
    {
        REQUIRE(wfmt(L"hello world") == L"hello world");
        REQUIRE(wfmt(L"{{not a field}}") == L"{not a field}");
        REQUIRE(wfmt(L"a{{b}}c") == L"a{b}c");
    }

    SECTION("auto indexing")
    {
        REQUIRE(wfmt(L"{} + {} = {}", 1, 2, 3) == L"1 + 2 = 3");
    }

    SECTION("manual indexing and reuse")
    {
        REQUIRE(wfmt(L"{0}{1}{0}", L"a", L"b") == L"aba");
    }

    SECTION("integer specs")
    {
        REQUIRE(wfmt(L"{:04}", 42) == L"0042");
        REQUIRE(wfmt(L"{:>6}", 42) == L"    42");
        REQUIRE(wfmt(L"{:<6}|", 42) == L"42    |");
        REQUIRE(wfmt(L"{:^6}", 42) == L"  42  ");
        REQUIRE(wfmt(L"{:*^7}", 42) == L"**42***");
        REQUIRE(wfmt(L"{:#x}", 255) == L"0xff");
        REQUIRE(wfmt(L"{:#X}", 255) == L"0XFF");
        REQUIRE(wfmt(L"{:#b}", 5) == L"0b101");
        REQUIRE(wfmt(L"{:08x}", 255) == L"000000ff");
        REQUIRE(wfmt(L"{:+}", 42) == L"+42");
        REQUIRE(wfmt(L"{}", -42) == L"-42");
        REQUIRE(wfmt(L"{:06}", -42) == L"-00042");
    }

    SECTION("string specs")
    {
        REQUIRE(wfmt(L"{:>8}", L"hi") == L"      hi");
        REQUIRE(wfmt(L"{:.3}", L"truncated") == L"tru");
    }

    SECTION("mixed argument types")
    {
        REQUIRE(wfmt(L"{} {} {}", std::wstring(L"str"), std::wstring_view(L"view"), L"lit") == L"str view lit");
        REQUIRE(wfmt(L"{}", 3.5) == L"3.5");
        REQUIRE(wfmt(L"{:.2f}", 3.14159) == L"3.14");
        REQUIRE(wfmt(L"{}", true) == L"true");
        REQUIRE(wfmt(L"{}", L'Z') == L"Z");
    }
}

TEST_CASE("WilFormat::CustomWrapper", "[resource][format]")
{
    unsigned int v = 255;
    REQUIRE(wfmt(L"val={}", wil_experimental::as_hex8(v)) == L"val=0x000000FF");
}

TEST_CASE("WilFormat::StringMakerTarget", "[resource][format]")
{
    // Literal format string flows through the compile-time-validated entry point
    // (format_nothrow) where available; a runtime string would use vformat_nothrow.
    wil::unique_cotaskmem_string result;
    REQUIRE(SUCCEEDED(wil_experimental::format_nothrow(result, L"{}-{}", 7, L"x")));
    REQUIRE(wcscmp(result.get(), L"7-x") == 0);
}

TEST_CASE("WilFormat::Errors", "[resource][format]")
{
    wil::unique_cotaskmem_string s;
    // Malformed runtime format strings surface as E_INVALIDARG from the runtime
    // (vformat) path. The equivalent literals are rejected at compile time when
    // compile-time validation is available (see WilFormat::Validate).
    REQUIRE(wil_experimental::vformat_nothrow(s, L"{1}", 1) == E_INVALIDARG);
    REQUIRE(wil_experimental::vformat_nothrow(s, L"{") == E_INVALIDARG);
    REQUIRE(wil_experimental::vformat_nothrow(s, L"}") == E_INVALIDARG);
    REQUIRE(wil_experimental::vformat_nothrow(s, L"{:q}", 1) == E_INVALIDARG); // bad type
}

// Compile-time validation of the format string against the argument count.
// validate() is constexpr, so these are checked by the compiler (static_assert).
static_assert(wil_experimental::validate(L"{} + {} = {}", 3), "three auto fields, three args");
static_assert(wil_experimental::validate(L"{0}{1}{0}", 2), "manual indexing in range");
static_assert(wil_experimental::validate(L"{{escaped}} {}", 1), "escaped braces plus one field");
static_assert(!wil_experimental::validate(L"{", 1), "unterminated field");
static_assert(!wil_experimental::validate(L"}", 0), "stray close brace");
static_assert(!wil_experimental::validate(L"{2}", 1), "index out of range");
static_assert(!wil_experimental::validate(L"{} {} {}", 2), "more fields than args");
static_assert(!wil_experimental::validate(L"{0} {}", 2), "mixed manual and automatic indexing");

// Per-type spec validation (validate_types). The spec must be sensible for the
// static type of the argument at that position.
static_assert(wil_experimental::validate_types<int>(L"{:x}"), "hex is valid for int");
static_assert(!wil_experimental::validate_types<const wchar_t*>(L"{:x}"), "hex is not valid for a string");
static_assert(!wil_experimental::validate_types<int>(L"{:.2f}"), "float spec is not valid for int");
static_assert(wil_experimental::validate_types<double>(L"{:.2f}"), "precision + 'f' is valid for double");
static_assert(!wil_experimental::validate_types<int>(L"{3}"), "index out of range for a single int arg");
static_assert(wil_experimental::validate_types<int, const wchar_t*>(L"{0:d} {1:>5}"), "matching specs per type");
static_assert(!wil_experimental::validate_types<const wchar_t*>(L"{:+}"), "sign is not valid for a string");

TEST_CASE("WilFormat::Validate", "[resource][format]")
{
    // STATIC_REQUIRE performs a static_assert (and registers a passing check).
    STATIC_REQUIRE(wil_experimental::validate(L"{}-{}", 2));
    STATIC_REQUIRE(wil_experimental::validate(L"no fields here", 0));
    STATIC_REQUIRE(wil_experimental::validate(L"{:>8}", 1));

    STATIC_REQUIRE_FALSE(wil_experimental::validate(L"{", 1));
    STATIC_REQUIRE_FALSE(wil_experimental::validate(L"}", 0));
    STATIC_REQUIRE_FALSE(wil_experimental::validate(L"{5}", 1));
    STATIC_REQUIRE_FALSE(wil_experimental::validate(L"{} {} {}", 2));
    STATIC_REQUIRE_FALSE(wil_experimental::validate(L"{0} {}", 2));

    // Per-type spec validation.
    STATIC_REQUIRE(wil_experimental::validate_types<int>(L"{:#06x}"));
    STATIC_REQUIRE(wil_experimental::validate_types<double>(L"{:+.3e}"));
    STATIC_REQUIRE_FALSE(wil_experimental::validate_types<int>(L"{:s}"));      // 's' not valid for int
    STATIC_REQUIRE_FALSE(wil_experimental::validate_types<double>(L"{:x}"));   // 'x' not valid for double
    STATIC_REQUIRE_FALSE(wil_experimental::validate_types<const wchar_t*>(L"{:08}")); // numeric flags on string

    // validate() is also usable at runtime.
    REQUIRE(wil_experimental::validate(L"{} {}", 2));
    REQUIRE_FALSE(wil_experimental::validate(L"{}", 0));
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("WilFormat::ThrowingWrapper", "[resource][format]")
{
    REQUIRE(wil_experimental::format(L"{} + {} = {}", 1, 2, 3) == L"1 + 2 = 3");

    // An invalid literal such as format(L"{1}", 1) is rejected at compile time
    // when compile-time validation is available, so it is not exercised here.
    // Runtime-string error handling is covered by WilFormat::Errors.
}
#endif // WIL_ENABLE_EXCEPTIONS

