#include <catch2/catch.hpp>
#include <e/system/native_string.h>
TEST_CASE("native string", "[native_string]")
{
    CHECK(wcscmp(e::system::ToNativeWideString("��������").get(), L"��������") == 0);
    CHECK(strcmp(e::system::ToNativeUtf8String("��������").get(), u8"��������") == 0);
}