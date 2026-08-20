#include "TestHarness.h"

#include "../src/data/json.h"

void TestJsonParsePrimitives()
{
    Json val;
    CHECK(Json::Parse(L"null", val));
    CHECK(val.isNull());

    CHECK(Json::Parse(L"true", val));
    CHECK(val.AsBool() == true);

    CHECK(Json::Parse(L"false", val));
    CHECK(val.AsBool() == false);

    CHECK(Json::Parse(L"0", val));
    CHECK(val.AsNumber() == 0.0);

    CHECK(Json::Parse(L"42", val));
    CHECK(val.AsNumber() == 42.0);

    CHECK(Json::Parse(L"-7", val));
    CHECK(val.AsNumber() == -7.0);

    CHECK(Json::Parse(L"3.14", val));
    CHECK(val.AsNumber() > 3.13 && val.AsNumber() < 3.15);

    CHECK(Json::Parse(L"1e10", val));
    CHECK(val.AsNumber() == 1e10);

    CHECK(Json::Parse(L"3.14e-2", val));
    CHECK(val.AsNumber() > 0.0313 && val.AsNumber() < 0.0315);

    CHECK(Json::Parse(L"-0", val));
    CHECK(val.AsNumber() == 0.0);
}

void TestJsonParseStrings()
{
    Json val;
    CHECK(Json::Parse(L"\"\"", val));
    CHECK(val.AsString() == L"");

    CHECK(Json::Parse(L"\"hello\"", val));
    CHECK(val.AsString() == L"hello");

    CHECK(Json::Parse(L"\"line1\\nline2\"", val));
    CHECK(val.AsString() == L"line1\nline2");

    CHECK(Json::Parse(L"\"tab\\there\"", val));
    CHECK(val.AsString() == L"tab\there");

    CHECK(Json::Parse(L"\"back\\\\slash\"", val));
    CHECK(val.AsString() == L"back\\slash");

    CHECK(Json::Parse(L"\"quo\\\"te\"", val));
    CHECK(val.AsString() == L"quo\"te");

    CHECK(Json::Parse(L"\"slash\\/ok\"", val));
    CHECK(val.AsString() == L"slash/ok");

    CHECK(Json::Parse(L"\"cr\\r\"", val));
    CHECK(val.AsString() == L"cr\r");

    CHECK(Json::Parse(L"\"bs\\b\"", val));
    CHECK(val.AsString() == L"bs\b");

    CHECK(Json::Parse(L"\"ff\\f\"", val));
    CHECK(val.AsString() == L"ff\f");

    CHECK(Json::Parse(L"\"uni\\u0041\"", val));
    CHECK(val.AsString() == L"uniA");

    CHECK(Json::Parse(L"\"zh\\u4e16\"", val));
    CHECK(val.AsString() == L"zh\x4E16");

    CHECK(Json::Parse(L"\" surrogate \\uD834\\uDD1E end\"", val));
    CHECK(val.AsString() == L" surrogate \xD834\xDD1E end");
}

void TestJsonParseStructures()
{
    Json val;

    CHECK(Json::Parse(L"{}", val));
    CHECK(val.type() == Json::Type::Object);
    CHECK(val.Size() == 0);

    CHECK(Json::Parse(L"{\"a\": 1}", val));
    CHECK(val.Has(L"a"));
    CHECK(val.At(L"a").AsNumber() == 1.0);

    CHECK(Json::Parse(L"{\"x\": 1, \"y\": 2}", val));
    CHECK(val.Size() == 2);
    CHECK(val.At(L"x").AsNumber() == 1.0);
    CHECK(val.At(L"y").AsNumber() == 2.0);

    CHECK(Json::Parse(L"{\"n\": null, \"b\": true, \"s\": \"hi\"}", val));
    CHECK(val.At(L"n").isNull());
    CHECK(val.At(L"b").AsBool() == true);
    CHECK(val.At(L"s").AsString() == L"hi");

    CHECK(Json::Parse(L"{\"nested\": {\"inner\": 42}}", val));
    CHECK(val.At(L"nested").At(L"inner").AsNumber() == 42.0);

    CHECK(Json::Parse(L"[]", val));
    CHECK(val.type() == Json::Type::Array);
    CHECK(val.Size() == 0);

    CHECK(Json::Parse(L"[1, 2, 3]", val));
    CHECK(val.Size() == 3);
    CHECK(val.At(0).AsNumber() == 1.0);
    CHECK(val.At(1).AsNumber() == 2.0);
    CHECK(val.At(2).AsNumber() == 3.0);

    CHECK(Json::Parse(L"[1, \"two\", true, null]", val));
    CHECK(val.Size() == 4);
    CHECK(val.At(0).AsNumber() == 1.0);
    CHECK(val.At(1).AsString() == L"two");
    CHECK(val.At(2).AsBool() == true);
    CHECK(val.At(3).isNull());

    CHECK(Json::Parse(L"[[1, 2], [3, 4]]", val));
    CHECK(val.Size() == 2);
    CHECK(val.At(0).At(0).AsNumber() == 1.0);
    CHECK(val.At(1).At(1).AsNumber() == 4.0);

    CHECK(Json::Parse(L"{\"arr\": [10, {\"k\": \"v\"}]}", val));
    CHECK(val.At(L"arr").At(1).At(L"k").AsString() == L"v");
}

void TestJsonParseEdgeCases()
{
    Json val;
    CHECK(!Json::Parse(L"", val));
    CHECK(!Json::Parse(L"   ", val));
    CHECK(!Json::Parse(L"{,}", val));
    CHECK(!Json::Parse(L"[1,]", val));
    CHECK(!Json::Parse(L"{\"a\":1}extra", val));
    CHECK(!Json::Parse(L"{a: 1}", val));
    CHECK(!Json::Parse(L"{'a': 1}", val));
    CHECK(!Json::Parse(L"NaN", val));
    CHECK(!Json::Parse(L"Infinity", val));
    CHECK(!Json::Parse(L"\"unterminated", val));
    CHECK(!Json::Parse(L"[1, 2", val));
    CHECK(!Json::Parse(L"{\"a\": 1", val));
    CHECK(!Json::Parse(L"tru", val));
    CHECK(!Json::Parse(L"fals", val));
    CHECK(!Json::Parse(L"nul", val));
}

void TestJsonParseDepthLimit()
{
    // A settings/layout file with a deeply nested array or object (corrupted
    // or hand-edited) must fail to parse rather than crash the process via
    // stack overflow in the recursive-descent parser.
    Json val;

    std::wstring deepArray(250, L'[');
    deepArray.append(250, L']');
    CHECK(!Json::Parse(deepArray, val));

    std::wstring deepObject;
    for (int i = 0; i < 250; ++i)
    {
        deepObject += L"{\"a\":";
    }
    deepObject += L"1";
    for (int i = 0; i < 250; ++i)
    {
        deepObject += L"}";
    }
    CHECK(!Json::Parse(deepObject, val));

    // Nesting comfortably under the limit must still parse fine.
    std::wstring shallowArray(50, L'[');
    shallowArray.append(50, L']');
    CHECK(Json::Parse(shallowArray, val));
}

void TestJsonRoundTrip()
{
    const std::wstring inputs[] = {
        L"null",
        L"true",
        L"false",
        L"42",
        L"-7",
        L"3.14",
        L"\"hello world\"",
        L"\"line\\nbreak\"",
        L"\"back\\\\slash\"",
        L"\"quo\\\"te\"",
        L"{}",
        L"{\"a\":1,\"b\":2}",
        L"[]",
        L"[1,2,3]",
        L"{\"n\":{\"arr\":[true,false,null]}}",
    };
    for (const auto& input : inputs)
    {
        Json parsed;
        CHECK(Json::Parse(input, parsed));
        const std::wstring compact = parsed.SerializeCompact();
        Json reparsed;
        CHECK(Json::Parse(compact, reparsed));
        CHECK(parsed.SerializeCompact() == reparsed.SerializeCompact());
    }
}

void TestJsonApi()
{
    Json obj;
    obj.Set(L"a", 1.0);
    obj.Set(L"b", true);
    obj.Set(L"c", Json::MakeString(L"hello"));
    CHECK(obj.Has(L"a"));
    CHECK(obj.Has(L"b"));
    CHECK(obj.Has(L"c"));
    CHECK(!obj.Has(L"d"));
    CHECK(obj.Size() == 3);
    CHECK(obj.At(L"a").AsNumber() == 1.0);
    CHECK(obj.At(L"b").AsBool() == true);
    CHECK(obj.At(L"c").AsString() == L"hello");
    CHECK(!obj.Has(L"x"));

    obj.Set(L"a", 99.0);
    CHECK(obj.At(L"a").AsNumber() == 99.0);
    CHECK(obj.Size() == 3);

    Json arr;
    arr.Push(Json::MakeNumber(1));
    arr.Push(Json::MakeNumber(2));
    arr.Push(Json::MakeNumber(3));
    CHECK(arr.Size() == 3);
    CHECK(arr.At(0).AsNumber() == 1.0);
    CHECK(arr.At(2).AsNumber() == 3.0);
    CHECK(arr.At(99).isNull());

    Json def;
    CHECK(def.isNull());
    CHECK(def.AsNumber(5.0) == 5.0);
    CHECK(def.AsBool(true) == true);
    CHECK(def.AsString(L"fallback") == L"fallback");
    CHECK(def.Size() == 0);
}

void TestJsonSerializeIndented()
{
    Json obj = Json::MakeObject();
    obj.Set(L"name", Json::MakeString(L"test"));
    obj.Set(L"count", 42.0);
    const std::wstring indented = obj.SerializeIndented();
    CHECK(indented.find(L'\n') != std::wstring::npos);
    CHECK(indented.find(L"  ") != std::wstring::npos);
    CHECK(indented.find(L"\"name\"") != std::wstring::npos);
    CHECK(indented.find(L"\"test\"") != std::wstring::npos);
    CHECK(indented.find(L"42") != std::wstring::npos);
}

void RunJsonTests()
{
    TestJsonParsePrimitives();
    TestJsonParseStrings();
    TestJsonParseStructures();
    TestJsonParseEdgeCases();
    TestJsonParseDepthLimit();
    TestJsonRoundTrip();
    TestJsonApi();
    TestJsonSerializeIndented();
}
