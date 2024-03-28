// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "config.h"

#include <cstring>
#include <iostream>
#include <fstream>

#include "base/test_minimal.h"
#include "base/test_help.h"
#include "game/tilemap.h"
#include "game/loader.h"
#include "data/json.h"

class TestVectorData : public game::TilemapData
{
public:
    virtual void Write(const void* ptr, size_t bytes, size_t offset) override
    {
        TEST_REQUIRE(offset + bytes <= mBytes.size());

        std::memcpy(&mBytes[offset], ptr, bytes);
    }
    virtual void Read(void* ptr, size_t bytes, size_t offset) const override
    {
        TEST_REQUIRE(offset + bytes <= mBytes.size());

        std::memcpy(ptr, &mBytes[offset], bytes);
    }
    virtual size_t AppendChunk(size_t bytes) override
    {
        const auto offset = mBytes.size();
        mBytes.resize(offset + bytes);
        return offset;
    }
    virtual void Resize(size_t bytes) override
    {
        mBytes.resize(bytes);
    }
    virtual void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) override
    {
        TEST_REQUIRE(offset + value_size * num_values <= mBytes.size());

        for (size_t i=0; i<num_values; ++i)
        {
            const auto buffer_offset = offset + i * value_size;
            TEST_REQUIRE(buffer_offset + value_size <= mBytes.size());
            std::memcpy(&mBytes[buffer_offset], value, value_size);
        }
    }
    virtual size_t GetByteCount() const override
    {
        return mBytes.size();
    }

    void Dump(const std::string& file) const
    {
        std::ofstream out;
        out.open(file, std::ios::out | std::ios::binary);
        TEST_REQUIRE(out.is_open());
        out.write((const char*)(&mBytes[0]), mBytes.size());
    }
private:
    std::vector<unsigned char> mBytes;
    unsigned mNumRows = 0;
    unsigned mNumCols = 0;
    size_t mValSize = 0;
};


void test_details()
{
    TEST_CASE(test::Type::Feature)

    namespace det = game::detail;

    {
        det::Render_Tile t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 123));

        uint8_t index;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(index == 123);
    }

    // uint4
    {
        det::Render_Data_Tile_UInt4 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 15));
        TEST_REQUIRE(det::SetTileValue(t, 15));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 15);
        TEST_REQUIRE(value == 15);

        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);
        TEST_REQUIRE(det::SetTileValue(t, 0));
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }

    // sint4 positive
    {
        det::Render_Data_Tile_SInt4 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 15));
        TEST_REQUIRE(det::SetTileValue(t, 7));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 15);
        TEST_REQUIRE(value == 7);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);
    }

    // sint4 negative
    {
        det::Render_Data_Tile_SInt4 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 15));
        TEST_REQUIRE(det::SetTileValue(t, -8));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 15);
        TEST_REQUIRE(value == -8);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }

    // uint8
    {
        det::Render_Data_Tile_UInt8 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 123));
        TEST_REQUIRE(det::SetTileValue(t, 255));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 123);
        TEST_REQUIRE(value == 255);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);
        TEST_REQUIRE(det::SetTileValue(t, 0));
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);

    }

    // sint8
    {
        det::Render_Data_Tile_SInt8 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 123));
        TEST_REQUIRE(det::SetTileValue(t, 127));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 123);
        TEST_REQUIRE(value == 127);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);
    }

    // sint8
    {
        det::Render_Data_Tile_SInt8 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 123));
        TEST_REQUIRE(det::SetTileValue(t, -128));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 123);
        TEST_REQUIRE(value == -128);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }

    // uint24
    {
        det::Render_Data_Tile_UInt24 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 123));
        TEST_REQUIRE(det::SetTileValue(t, 0xffffff));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 123);
        TEST_REQUIRE(value == 0xffffff);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);

        TEST_REQUIRE(det::SetTileValue(t, 0));
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }

    // sint24
    {
        det::Render_Data_Tile_SInt24 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 123));
        TEST_REQUIRE(det::SetTileValue(t, 0x7fffff));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 123);
        TEST_REQUIRE(value == 0x7fffff);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);
    }

    // sint24
    {
        det::Render_Data_Tile_SInt24 t;
        TEST_REQUIRE(det::SetTilePaletteIndex(t, 123));
        TEST_REQUIRE(det::SetTileValue(t, -0x800000));

        uint8_t index;
        int32_t value;
        TEST_REQUIRE(det::GetTilePaletteIndex(t, &index));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(index == 123);
        TEST_REQUIRE(value == -0x800000);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }


    {
        det::Data_Tile_SInt8 t;
        TEST_REQUIRE(det::SetTileValue(t, 127));
        int32_t value;
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(value == 127);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);

        TEST_REQUIRE(det::SetTileValue(t, -128));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(value == -128);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }

    {
        det::Data_Tile_UInt8 t;
        TEST_REQUIRE(det::SetTileValue(t, 255));
        int32_t value;
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(value == 255);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);

        TEST_REQUIRE(det::SetTileValue(t, 0));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(value == 0);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }

    {
        det::Data_Tile_SInt16 t;
        TEST_REQUIRE(det::SetTileValue(t, 0x7fff));
        int32_t value;
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(value == 0x7fff);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);

        TEST_REQUIRE(det::SetTileValue(t, -0x8000));
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(value == -0x8000);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }

    {
        det::Data_Tile_UInt16  t;
        TEST_REQUIRE(det::SetTileValue(t, 0xffff));
        int32_t value;
        TEST_REQUIRE(det::GetTileValue(t, &value));
        TEST_REQUIRE(value == 0xffff);
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 1.0f);
        TEST_REQUIRE(det::SetTileValue(t, 0));
        TEST_REQUIRE(det::NormalizeTileDataValue(t) == 0.0f);
    }
}


void test_tilemap_layer()
{
    TEST_CASE(test::Type::Feature)

    game::TilemapLayerClass klass;
    klass.SetName("foobar");
    klass.SetId("1231xxx");
    klass.SetReadOnly(true);
    klass.SetStorage(game::TilemapLayerClass::Storage::Sparse);
    klass.SetType(game::TilemapLayerClass::Type::DataUInt16);
    klass.SetCache(game::TilemapLayerClass::Cache::Cache128);
    klass.SetResolution(game::TilemapLayerClass::Resolution::DownScale8);
    klass.SetDefaultTileValue(game::detail::Data_Tile_UInt16 {5});
    klass.SetDataUri("pck://foobar/data.bin");
    klass.SetPaletteMaterialId("some_material", 0);
    klass.SetPaletteMaterialId("other_material", 1);
    klass.SetDepth(-2);

    {
        data::JsonObject json;
        klass.IntoJson(json);
        //std::cout << json.ToString();

        game::TilemapLayerClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetHash() == klass.GetHash());
        TEST_REQUIRE(ret.GetName() == "foobar");
        TEST_REQUIRE(ret.GetId()  == "1231xxx");
        TEST_REQUIRE(ret.IsReadOnly());
        TEST_REQUIRE(ret.GetStorage() == game::TilemapLayerClass::Storage::Sparse);
        TEST_REQUIRE(ret.GetType() == game::TilemapLayerClass::Type::DataUInt16);
        TEST_REQUIRE(ret.GetCache() == game::TilemapLayerClass::Cache::Cache128);
        TEST_REQUIRE(ret.GetResolution() == game::TilemapLayerClass::Resolution::DownScale8);
        TEST_REQUIRE(ret.GetDefaultTileValue<game::detail::Data_Tile_UInt16>().data == 5);
        TEST_REQUIRE(ret.GetDataUri() == "pck://foobar/data.bin");
        TEST_REQUIRE(ret.GetPaletteMaterialId(0) == "some_material");
        TEST_REQUIRE(ret.GetPaletteMaterialId(1) == "other_material");
        TEST_REQUIRE(ret.GetDepth() == -2);
    }

    // copy
    {
        game::TilemapLayerClass copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    {
        auto data = std::make_shared<TestVectorData>();
        auto klass = std::make_shared<game::TilemapLayerClass>();
        klass->SetType(game::TilemapLayerClass::Type::Render);
        klass->SetDefaultTilePaletteMaterialIndex(40);
        klass->Initialize(1, 1, *data);

        auto inst = game::CreateTilemapLayer(klass, 1, 1);
        inst->Load(data, 1024);

        uint8_t palette_index = 0;
        TEST_REQUIRE(inst->GetTilePaletteIndex(&palette_index, 0, 0));
        TEST_REQUIRE(palette_index == 40);
        TEST_REQUIRE(inst->SetTilePaletteIndex(20, 0, 0));
        TEST_REQUIRE(inst->GetTilePaletteIndex(&palette_index, 0, 0));
        TEST_REQUIRE(palette_index == 20);
    }

    {
        auto data = std::make_shared<TestVectorData>();
        auto klass = std::make_shared<game::TilemapLayerClass>();
        klass->SetType(game::TilemapLayerClass::Type::Render_DataUInt8);
        klass->SetDefaultTilePaletteMaterialIndex(40);
        klass->SetDefaultTileDataValue(123);
        klass->Initialize(1, 1, *data);

        auto inst = game::CreateTilemapLayer(klass, 1, 1);
        inst->Load(data, 1024);

        uint8_t palette_index = 0;
        int32_t tile_value = 0;
        TEST_REQUIRE(inst->GetTilePaletteIndex(&palette_index, 0, 0));
        TEST_REQUIRE(inst->GetTileValue(&tile_value, 0, 0));
        TEST_REQUIRE(palette_index == 40);
        TEST_REQUIRE(tile_value == 123);
        TEST_REQUIRE(inst->SetTilePaletteIndex(20, 0, 0));
        TEST_REQUIRE(inst->SetTileValue(34, 0, 0));
        TEST_REQUIRE(inst->GetTilePaletteIndex(&palette_index, 0, 0));
        TEST_REQUIRE(inst->GetTileValue(&tile_value, 0, 0));
        TEST_REQUIRE(palette_index == 20);
        TEST_REQUIRE(tile_value == 34);
    }
}

void test_tilemap_class()
{
    TEST_CASE(test::Type::Feature)

    game::TilemapClass klass;
    klass.SetName("foobar");
    klass.SetTileWidth(5.0f);
    klass.SetTileHeight(8.0f);
    klass.SetTileDepth(10.0f);
    klass.SetMapWidth(200);
    klass.SetMapHeight(240);
    klass.SetTileRenderScale({2.0f, 3.0f});
    klass.SetScriptFile("foobar.lua");

    game::TilemapLayerClass layer0;
    layer0.SetName("layer0");
    layer0.SetType(game::TilemapLayerClass::Type::Render);
    klass.AddLayer(layer0);

    game::TilemapLayerClass layer1;
    layer0.SetName("layer1");
    layer0.SetType(game::TilemapLayerClass::Type::DataUInt8);
    klass.AddLayer(layer1);

    {
        data::JsonObject json;
        klass.IntoJson(json);
        game::TilemapClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetHash() == klass.GetHash());
        TEST_REQUIRE(ret.GetName() == "foobar");
        TEST_REQUIRE(ret.GetScriptFile() == "foobar.lua");
        TEST_REQUIRE(ret.GetMapWidth() == 200);
        TEST_REQUIRE(ret.GetMapHeight() == 240);
        TEST_REQUIRE(ret.GetTileWidth() == 5.0f);
        TEST_REQUIRE(ret.GetTileHeight() == 8.0f);
        TEST_REQUIRE(ret.GetTileDepth() == 10.0f);
        TEST_REQUIRE(ret.GetTileRenderScale() == glm::vec2(2.0f, 3.0f));
    }

    {
        game::TilemapClass copy(klass);
        TEST_REQUIRE(copy.GetHash() == klass.GetHash());
    }

    {
        auto clone = klass.Clone();
        TEST_REQUIRE(clone.GetHash() != klass.GetHash());
        TEST_REQUIRE(clone.GetName() == "foobar");
        TEST_REQUIRE(clone.GetScriptFile() == "foobar.lua");
        TEST_REQUIRE(clone.GetMapWidth() == 200);
        TEST_REQUIRE(clone.GetMapHeight() == 240);
        TEST_REQUIRE(clone.GetTileWidth() == 5.0f);
        TEST_REQUIRE(clone.GetTileHeight() == 8.0f);
        TEST_REQUIRE(clone.GetTileRenderScale() == glm::vec2(2.0f, 3.0f));
    }

    // assignment
    {
        game::TilemapClass foo;
        foo = klass;
        TEST_REQUIRE(foo.GetHash() == klass.GetHash());
    }
}

void test_mixed_resolution_tile_access()
{
    TEST_CASE(test::Type::Feature)

    auto map = std::make_shared<game::TilemapClass>();
    map->SetMapWidth(100);
    map->SetMapHeight(50);

    {
        auto layer = std::make_shared<game::TilemapLayerClass>();
        layer->SetStorage(game::TilemapLayerClass::Storage::Dense);
        layer->SetResolution(game::TilemapLayerClass::Resolution::Original);
        layer->SetCache(game::TilemapLayerClass::Cache::Automatic);
        layer->SetType(game::TilemapLayerClass::Type::DataUInt8);
        map->AddLayer(layer);
    }

    {
        auto layer = std::make_shared<game::TilemapLayerClass>();
        layer->SetStorage(game::TilemapLayerClass::Storage::Dense);
        layer->SetResolution(game::TilemapLayerClass::Resolution::DownScale2);
        layer->SetCache(game::TilemapLayerClass::Cache::Automatic);
        layer->SetType(game::TilemapLayerClass::Type::DataUInt8);
        map->AddLayer(layer);
    }

    {
        auto layer = std::make_shared<game::TilemapLayerClass>();
        layer->SetStorage(game::TilemapLayerClass::Storage::Dense);
        layer->SetResolution(game::TilemapLayerClass::Resolution::UpScale2);
        layer->SetCache(game::TilemapLayerClass::Cache::Automatic);
        layer->SetType(game::TilemapLayerClass::Type::DataUInt8);
        map->AddLayer(layer);
    }

    {
        auto data = std::make_shared<TestVectorData>();
        auto klass = map->GetSharedLayerClass(0);
        klass->Initialize(map->GetMapWidth(),
                          map->GetMapHeight(), *data);
        auto layer = game::CreateTilemapLayer(klass,
                                              map->GetMapWidth(),
                                              map->GetMapHeight());
        layer->Load(data, 0);

        TEST_REQUIRE((*layer)->GetResolution() == game::TilemapLayerClass::Resolution::Original);
        TEST_REQUIRE(layer->GetWidth() == 100);
        TEST_REQUIRE(layer->GetHeight() == 50);
        TEST_REQUIRE(layer->SetTileValue(1, 0, 0));
        TEST_REQUIRE(layer->SetTileValue(1, 49, 99));
    }

    {
        auto data = std::make_shared<TestVectorData>();
        auto klass = map->GetSharedLayerClass(1);
        klass->Initialize(map->GetMapWidth(),
                          map->GetMapHeight(), *data);
        auto layer = game::CreateTilemapLayer(klass,
                                              map->GetMapWidth(),
                                              map->GetMapHeight());
        layer->Load(data, 0);

        TEST_REQUIRE((*layer)->GetResolution() == game::TilemapLayerClass::Resolution::DownScale2);
        TEST_REQUIRE(layer->GetWidth() == 50);
        TEST_REQUIRE(layer->GetHeight() == 25);
        TEST_REQUIRE(layer->SetTileValue(1, 0, 0));
        TEST_REQUIRE(layer->SetTileValue(1, 24, 49));
    }

    {
        auto data = std::make_shared<TestVectorData>();
        auto klass = map->GetSharedLayerClass(2);
        klass->Initialize(map->GetMapWidth(),
                          map->GetMapHeight(), *data);
        auto layer = game::CreateTilemapLayer(klass,
                                              map->GetMapWidth(),
                                              map->GetMapHeight());
        layer->Load(data, 0);

        TEST_REQUIRE((*layer)->GetResolution() == game::TilemapLayerClass::Resolution::UpScale2);
        TEST_REQUIRE(layer->GetWidth() == 200);
        TEST_REQUIRE(layer->GetHeight() == 100);
        TEST_REQUIRE(layer->SetTileValue(1, 0, 0));
        TEST_REQUIRE(layer->SetTileValue(1, 99, 199));
    }

}

void test_tile_access_basic(game::TilemapLayerClass::Storage storage)
{
    TEST_CASE(test::Type::Feature)

    auto klass = std::make_shared<game::TilemapLayerClass>();
    klass->SetStorage(storage);
    klass->SetResolution(game::TilemapLayerClass::Resolution::Original);
    klass->SetCache(game::TilemapLayerClass::Cache::Cache64);
    klass->SetType(game::TilemapLayerClass::Type::DataUInt8);
    klass->SetDefaultTileValue(game::detail::Data_Tile_UInt8{42});

    const auto map_width  = 1024;
    const auto map_height = 512;
    auto layer = game::CreateTilemapLayer(klass, map_width, map_height);

    auto data = std::make_shared<TestVectorData>();
    // initialize the datastructures on the data object for supporting
    // the tile map layer.
    klass->Initialize(map_width, map_height, *data);

    // first tile cache gets loaded
    layer->Load(data, 0);

    auto* ptr = game::TilemapLayerCast<game::TilemapLayer_Data_UInt8>(layer);

    // the data hasn't been touched yet so we should get the
    // original data from the buffer.
    TEST_REQUIRE(ptr->GetTile(0, 0).data == 42);
    TEST_REQUIRE(ptr->GetTile(0, 1).data == 42);
    game::detail::Data_Tile_UInt8 tile;
    tile.data = 55;
    ptr->SetTile(tile, 0, 0);
    ptr->SetTile(tile, 0, 1);
    TEST_REQUIRE(ptr->GetTile(0, 0).data == 55);
    TEST_REQUIRE(ptr->GetTile(0, 1).data == 55);

    // jump onto next cache line
    TEST_REQUIRE(ptr->GetTile(0, 64).data == 42);
    // then refetch the first cache line
    TEST_REQUIRE(ptr->GetTile(0, 0).data == 55);
    TEST_REQUIRE(ptr->GetTile(0, 1).data == 55);
}

template<typename TileType>
void test_tile_access_sparse()
{
    TEST_CASE(test::Type::Feature)

    const auto type = game::detail::TilemapLayerTraits<TileType>::LayerType;

    auto klass = std::make_shared<game::TilemapLayerClass>();
    klass->SetStorage(game::TilemapLayerClass::Storage::Sparse);
    klass->SetResolution(game::TilemapLayerClass::Resolution::Original);
    klass->SetCache(game::TilemapLayerClass::Cache::Cache128);
    klass->SetType(type);

    TileType default_tile;
    default_tile.data = 20;
    klass->SetDefaultTileValue(default_tile);

    // this will map to 1x3 blocks (row x cols)
    const auto map_width  = 129;
    const auto map_height = 3;

    // if the block size changes in the implementation then
    // adapt this test case appropriately.
    unsigned block_width  = 0;
    unsigned block_height = 0;
    game::TilemapLayerClass::GetSparseBlockSize(klass->GetTileDataSize(),
                                                klass->MapDimension(map_width),
                                                klass->MapDimension(map_height),
                                                &block_width, &block_height);

    //TEST_REQUIRE()

    auto layer = game::CreateTilemapLayer(klass, map_width, map_height);

    auto data = std::make_shared<TestVectorData>();
    // initialize the datastructures on the data object for supporting
    // the tile map layer.
    klass->Initialize(map_width, map_height, *data);

    // first tile cache gets loaded
    layer->Load(data, 0);

    auto* ptr = game::TilemapLayerCast<game::detail::TilemapLayerBase<TileType>>(layer);

    TEST_REQUIRE(ptr->GetTile(0, 0).data == 20);
    TEST_REQUIRE(ptr->GetTile(0, 128).data == 20);
    TEST_REQUIRE(ptr->GetTile(2, 128).data == 20);

    TileType my_tile;
    my_tile.data = 55;

    ptr->SetTile(my_tile, 0, 0);
    ptr->SetTile(my_tile, 0, 128);
    ptr->SetTile(my_tile, 2, 128);

    TEST_REQUIRE(ptr->GetTile(0, 0).data == 55);
    TEST_REQUIRE(ptr->GetTile(0, 128).data == 55);
    TEST_REQUIRE(ptr->GetTile(2, 128).data == 55);

    TEST_REQUIRE(ptr->GetTile(0, 1).data == 20);
    TEST_REQUIRE(ptr->GetTile(0, 127).data == 20);
    TEST_REQUIRE(ptr->GetTile(1, 127).data == 20);
    TEST_REQUIRE(ptr->GetTile(2, 127).data == 20);
}

template<typename TileType>
void test_tile_access_combinations(game::TilemapLayerClass::Storage storage)
{
    TEST_CASE(test::Type::Feature)

    const auto type = game::detail::TilemapLayerTraits<TileType>::LayerType;

    using Cache = game::TilemapLayerClass::Cache;

    // access everything with various combinations

    struct TestCase {
        unsigned map_width  = 0;
        unsigned map_height = 0;
        Cache cache;
    } cases[] = {
        {100, 1, Cache::Cache128},
        {1, 100, Cache::Cache128},

        {129, 1, Cache::Cache32},
        {129, 1, Cache::Cache64},
        {129, 1, Cache::Cache128},
        {129, 1, Cache::Cache256},

        {1, 33, Cache::Cache32},
        {33, 1, Cache::Cache32},
        {1, 129, Cache::Cache32},
        {1, 129, Cache::Cache64},
        {1, 129, Cache::Cache128},
        {1, 129, Cache::Cache256},

        {1000, 399, Cache::Cache8},
        {512, 512, Cache::Cache32},
        {400, 500, Cache::Cache8},
        {100, 634, Cache::Cache128},
        {10, 10, Cache::Cache1024}
    };

    for (const auto& test : cases)
    {
        auto klass = std::make_shared<game::TilemapLayerClass>();
        klass->SetStorage(storage);
        klass->SetResolution(game::TilemapLayerClass::Resolution::Original);
        klass->SetType(type);
        klass->SetCache(test.cache);

        auto layer = game::CreateTilemapLayer(klass, test.map_width, test.map_height);
        auto data = std::make_shared<TestVectorData>();
        klass->Initialize(test.map_width, test.map_height, *data);

        // first tile cache gets loaded
        layer->Load(data, 0);
        auto* ptr = game::TilemapLayerCast<game::detail::TilemapLayerBase<TileType>>(layer);

        // sequential
        unsigned write_counter = 0;
        unsigned read_counter = 0;
        for (unsigned row=0; row<test.map_height; ++row)
        {
            for (unsigned col=0; col<test.map_width; ++col)
            {
                TileType tile;
                tile.data = write_counter++ % 256;
                ptr->SetTile(tile, row, col);
            }
        }
        for (unsigned row=0; row<test.map_height; ++row)
        {
            for (unsigned col=0; col<test.map_width; ++col)
            {
                const auto& tile = ptr->GetTile(row, col);
                const auto expected = read_counter++ % 256;
                TEST_REQUIRE(tile.data == expected);
            }
        }
    }
}


template<typename TileType>
void test_layer_save_load(game::TilemapLayerClass::Storage storage)
{
    TEST_CASE(test::Type::Feature)

    auto type = game::detail::TilemapLayerTraits<TileType>::LayerType;

    auto klass = std::make_shared<game::TilemapLayerClass>();
    klass->SetStorage(storage);
    klass->SetResolution(game::TilemapLayerClass::Resolution::Original);
    klass->SetCache(game::TilemapLayerClass::Cache::Cache64);
    klass->SetType(type);

    TileType default_tile;
    default_tile.data = 60;
    klass->SetDefaultTileValue(default_tile);

    const auto map_width  = 1024;
    const auto map_height = 512;

    auto data = std::make_shared<TestVectorData>();

    {
        auto layer = game::CreateTilemapLayer(klass, map_width, map_height);
        // initialize the datastructures on the data object for supporting
        // the tile map layer.
        klass->Initialize(map_width, map_height, *data);

        // first tile cache gets loaded
        layer->Load(data, 0);

        auto* ptr = game::TilemapLayerCast<game::detail::TilemapLayerBase<TileType>>(layer);
        TileType tile;
        tile.data = 55;
        ptr->SetTile(tile, 0, 0);
        ptr->SetTile(tile, 0, 1);
        ptr->SetTile(tile, 511, 1023);

        ptr->FlushCache();
        ptr->Save();
    }

    {
        auto layer = game::CreateTilemapLayer(klass, map_width, map_height);

        // first tile cache gets loaded
        layer->Load(data, 0);

        auto* ptr = game::TilemapLayerCast<game::detail::TilemapLayerBase<TileType>>(layer);
        TEST_REQUIRE(ptr->GetTile(0, 0).data == 55);
        TEST_REQUIRE(ptr->GetTile(0, 1).data == 55);
        TEST_REQUIRE(ptr->GetTile(511, 1023).data == 55);
    }
}

template<typename TileType>
void test_layer_resize(game::TilemapLayerClass::Storage storage)
{
    TEST_CASE(test::Type::Feature)

    struct TestCase {
        game::TilemapLayerClass::Resolution resolution;
        game::USize src_map_size;
        game::USize dst_map_size;
        game::USize src_layer_size;
        game::USize dst_layer_size;
    };

    const TestCase cases[] = {
        {
            game::TilemapLayerClass::Resolution::Original,
            game::USize {1000, 500},
            game::USize {1050, 500}, // width grows
            game::USize {1000, 500},
            game::USize {1050, 500},
        },
        {
            game::TilemapLayerClass::Resolution::Original,
            game::USize {1000, 500},
            game::USize {950,  500}, // width shrinks
            game::USize {1000, 500},
            game::USize {950,  500},
        },
        {
            game::TilemapLayerClass::Resolution::Original,
            game::USize {1000, 500},
            game::USize {1000, 550}, // height grows
            game::USize {1000, 500},
            game::USize {1000, 550},
        },
         {
            game::TilemapLayerClass::Resolution::Original,
            game::USize {1000, 500},
            game::USize {1000, 450}, // height shrinks
            game::USize {1000, 500},
            game::USize {1000, 450},
        },

        {
            game::TilemapLayerClass::Resolution::UpScale2,
            game::USize {1000, 500},
            game::USize {1050, 500}, // width grows
            game::USize {2000, 1000},
            game::USize {2100, 1000},
        },
        {
            game::TilemapLayerClass::Resolution::UpScale2,
            game::USize {1000, 500},
            game::USize {950,  500}, // width shrinks
            game::USize {2000, 1000},
            game::USize {1900, 1000},
        },
        {
            game::TilemapLayerClass::Resolution::UpScale2,
            game::USize {1000, 500},
            game::USize {1000, 550}, // height grows
            game::USize {2000, 1000},
            game::USize {2000, 1100},
        },
        {
            game::TilemapLayerClass::Resolution::UpScale2,
            game::USize {1000, 500},
            game::USize {1000, 450}, // height shrinks
            game::USize {2000, 1000},
            game::USize {2000, 900},
        },

        {
                game::TilemapLayerClass::Resolution::DownScale2,
                game::USize {1000, 500},
                game::USize {1050, 500}, // width grows
                game::USize {500, 250},
                game::USize {525, 250},
        },
        {
                game::TilemapLayerClass::Resolution::DownScale2,
                game::USize {1000, 500},
                game::USize {950,  500}, // width shrinks
                game::USize {500, 250},
                game::USize {475, 250},
        },
        {
                game::TilemapLayerClass::Resolution::DownScale2,
                game::USize {1000, 500},
                game::USize {1000, 550}, // height grows
                game::USize {500, 250},
                game::USize {500, 275},
        },
        {
                game::TilemapLayerClass::Resolution::DownScale2,
                game::USize {1000, 500},
                game::USize {1000, 450}, // height shrinks
                game::USize {500, 250},
                game::USize {500, 225},
        },

    };

    auto type = game::detail::TilemapLayerTraits<TileType>::LayerType;

    for (size_t i=0; i<base::ArraySize(cases); ++i)
    {
        const auto& test = cases[i];

        auto klass = std::make_shared<game::TilemapLayerClass>();
        klass->SetStorage(storage);
        klass->SetResolution(test.resolution);
        klass->SetCache(game::TilemapLayerClass::Cache::Cache64);
        klass->SetType(type);

        TileType default_tile;
        default_tile.data = 60;
        klass->SetDefaultTileValue(default_tile);

        const auto src_map_width  = test.src_map_size.GetWidth();
        const auto src_map_height = test.src_map_size.GetHeight();
        const auto src_layer_width = test.src_layer_size.GetWidth();
        const auto src_layer_height = test.src_layer_size.GetHeight();
        const auto dst_map_width  = test.dst_map_size.GetWidth();
        const auto dst_map_height = test.dst_map_size.GetHeight();
        const auto dst_layer_width  = test.dst_layer_size.GetWidth();
        const auto dst_layer_height = test.dst_layer_size.GetHeight();

        auto layer = game::CreateTilemapLayer(klass, src_map_width, src_map_height);
        TEST_REQUIRE(layer->GetWidth()  == src_layer_width);
        TEST_REQUIRE(layer->GetHeight() == src_layer_height);
        auto* ptr = game::TilemapLayerCast<game::detail::TilemapLayerBase<TileType>>(layer);

        auto data = std::make_shared<TestVectorData>();
        klass->Initialize(src_map_width, src_map_height, *data);
        layer->Load(data, 0);

        // fill the layer with data.
        // todo: use a non-constant value.
        for (unsigned row=0; row<src_layer_height; ++row)
        {
            for (unsigned col=0; col<src_layer_width; ++col)
            {
                TileType tile;
                tile.data = 55;
                ptr->SetTile(tile, row, col);
            }
        }
        ptr->FlushCache();
        ptr->Save();

        // apply changes in layer size.
        {
            // create new data buffer.
            auto resize = std::make_shared<TestVectorData>();
            // initialize the map data structure in the data buffer.
            klass->Initialize(dst_map_width, dst_map_height, *resize);
            // resize the map and copy data over from the old data buffer
            // into the new data buffer.
            klass->ResizeCopy(test.src_map_size,
                              test.dst_map_size,
                              *data, *resize);
            layer->SetMapDimensions(dst_map_width, dst_map_height);
            layer->Load(resize, 1024);

            TEST_REQUIRE(layer->GetWidth() == dst_layer_width);
            TEST_REQUIRE(layer->GetHeight() == dst_layer_height);

            // verify that the previously written data is still there.
            const auto min_row = std::min(src_layer_height, dst_layer_height);
            const auto min_col = std::min(src_layer_width, dst_layer_width);
            for (unsigned row=0; row<min_row; ++row)
            {
                for (unsigned col=0; col<min_col; ++col)
                {
                    TEST_REQUIRE(ptr->GetTile(row, col).data == 55);
                }
            }

            layer->FlushCache();
            layer->Save();
            data = resize;
        }

    }
}


template<typename Type>
void test_tilemaplayer_class_default_serialize(const Type& def)
{
    TEST_CASE(test::Type::Feature)

    const auto type = game::detail::TilemapLayerTraits<Type>::LayerType;

    game::TilemapLayerClass klass;
    klass.SetType(type);
    klass.SetDefaultTileValue(def);
    {
        data::JsonObject json;
        klass.IntoJson(json);
        //std::cout << json.ToString();

        game::TilemapLayerClass ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetType() == type);
        TEST_REQUIRE(ret.GetDefaultTileValue<Type>() == def);
    }
}

EXPORT_TEST_MAIN(
int test_main(int argc, char* argv[])
{

    using Type = game::TilemapLayerClass::Type;
    namespace det = game::detail;

    test_details();
    test_tilemap_layer();
    test_tilemap_class();

    test_mixed_resolution_tile_access();

    test_tile_access_basic(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Sparse);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Sparse);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Sparse);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Sparse);
    test_tile_access_basic(game::TilemapLayerClass::Storage::Sparse);

    test_tile_access_sparse<det::Render_Data_Tile_UInt8>();
    test_tile_access_sparse<det::Render_Data_Tile_UInt24>();
    test_tile_access_sparse<det::Data_Tile_UInt8 >();
    test_tile_access_sparse<det::Data_Tile_SInt16>();

    test_tile_access_combinations<det::Render_Data_Tile_UInt8>(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_combinations<det::Render_Data_Tile_UInt24>(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_combinations<det::Data_Tile_UInt8 >(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_combinations<det::Data_Tile_SInt16>(game::TilemapLayerClass::Storage::Dense);
    test_tile_access_combinations<det::Render_Data_Tile_UInt8>(game::TilemapLayerClass::Storage::Sparse);
    test_tile_access_combinations<det::Render_Data_Tile_UInt24>(game::TilemapLayerClass::Storage::Sparse);
    test_tile_access_combinations<det::Data_Tile_UInt8 >(game::TilemapLayerClass::Storage::Sparse);
    test_tile_access_combinations<det::Data_Tile_SInt16>(game::TilemapLayerClass::Storage::Sparse);

    test_layer_save_load<det::Render_Data_Tile_UInt8>(game::TilemapLayerClass::Storage::Dense);
    test_layer_save_load<det::Render_Data_Tile_UInt24>(game::TilemapLayerClass::Storage::Dense);
    test_layer_save_load<det::Data_Tile_UInt8 >(game::TilemapLayerClass::Storage::Dense);
    test_layer_save_load<det::Data_Tile_SInt16>(game::TilemapLayerClass::Storage::Dense);
    test_layer_save_load<det::Render_Data_Tile_UInt8>(game::TilemapLayerClass::Storage::Sparse);
    test_layer_save_load<det::Render_Data_Tile_UInt24>(game::TilemapLayerClass::Storage::Sparse);
    test_layer_save_load<det::Data_Tile_UInt8 >(game::TilemapLayerClass::Storage::Sparse);
    test_layer_save_load<det::Data_Tile_SInt16>(game::TilemapLayerClass::Storage::Sparse);

    test_layer_resize<det::Render_Data_Tile_UInt8>(game::TilemapLayerClass::Storage::Dense);
    test_layer_resize<det::Render_Data_Tile_UInt24>(game::TilemapLayerClass::Storage::Dense);
    test_layer_resize<det::Data_Tile_UInt8 >(game::TilemapLayerClass::Storage::Dense);
    test_layer_resize<det::Data_Tile_SInt16>(game::TilemapLayerClass::Storage::Dense);

    test_layer_resize<det::Render_Data_Tile_UInt8>(game::TilemapLayerClass::Storage::Sparse);
    test_layer_resize<det::Render_Data_Tile_UInt24>(game::TilemapLayerClass::Storage::Sparse);
    test_layer_resize<det::Data_Tile_UInt8 >(game::TilemapLayerClass::Storage::Sparse);
    test_layer_resize<det::Data_Tile_SInt16>(game::TilemapLayerClass::Storage::Sparse);

    test_tilemaplayer_class_default_serialize(det::Render_Tile{uint8_t(123)});
    test_tilemaplayer_class_default_serialize(det::Render_Tile{uint8_t(255)});
    test_tilemaplayer_class_default_serialize(det::Render_Data_Tile_UInt4{uint8_t(4), uint8_t(9)} );
    test_tilemaplayer_class_default_serialize(det::Render_Data_Tile_UInt4{uint8_t(15), uint8_t(0)} );
    test_tilemaplayer_class_default_serialize(det::Render_Data_Tile_UInt8{uint8_t(23), uint8_t(100)} );
    test_tilemaplayer_class_default_serialize(det::Render_Data_Tile_UInt8{uint8_t(23), uint8_t(255)} );
    test_tilemaplayer_class_default_serialize(det::Render_Data_Tile_SInt8{uint8_t(23), int8_t(-100)} );
    test_tilemaplayer_class_default_serialize(det::Render_Data_Tile_SInt8{uint8_t(23), int8_t(-128)} );
    test_tilemaplayer_class_default_serialize(det::Render_Data_Tile_SInt8{uint8_t(23), int8_t(100)}  );
    test_tilemaplayer_class_default_serialize(det::Render_Data_Tile_SInt8{uint8_t(23), int8_t(127)}  );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt8{int8_t(0)}    );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt8{int8_t(1)}    );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt8{int8_t(-1)}   );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt8{int8_t(-128)} );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt8{int8_t(127)}  );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_UInt8{uint8_t(0)}   );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_UInt8{uint8_t(127)} );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_UInt8{uint8_t(255)} );

    const auto min_s_16 = std::numeric_limits<int16_t>::min();
    const auto max_s_16 = std::numeric_limits<int16_t>::max();
    const auto min_u_16 = std::numeric_limits<uint16_t>::min();
    const auto max_u_16 = std::numeric_limits<uint16_t>::max();
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt16{int16_t(0)}         );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt16{int16_t(1)}         );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt16{int16_t(-1)}        );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt16{int16_t(-128)}      );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt16{int16_t(min_s_16)}  );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_SInt16{int16_t(max_s_16)}  );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_UInt16{uint16_t(0)}        );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_UInt16{uint16_t(min_u_16)} );
    test_tilemaplayer_class_default_serialize(det::Data_Tile_UInt16{uint16_t(max_u_16)} );

    return 0;
}
) //
