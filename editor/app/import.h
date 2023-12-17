// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <assimp/Importer.hpp>      // C++ importer interface
#  include <assimp/scene.h>           // Output data structure
#  include <assimp/postprocess.h>     // Post processing flags
#  include <glm/vec3.hpp>
#  include <QString>
#include "warnpop.h"

#include <memory>
#include <vector>
#include <unordered_map>

#include "editor/app/types.h"
#include "graphics/fwd.h"

namespace app
{
    class ModelImporter
    {
    public:
        struct DrawableInfo {
            AnyString name;
            AnyString material;
            unsigned vertices  = 0;
            unsigned triangles = 0 ;
            unsigned draw_cmd_start = 0;
            unsigned draw_cmd_count = 0;
        };

        struct MaterialInfo {
            AnyString key;
            std::shared_ptr<const gfx::MaterialClass> klass;
        };

        ModelImporter(const ModelImporter&) = delete;
        ModelImporter();
       ~ModelImporter();

        QAbstractTableModel* GetDrawableModel();
        QAbstractTableModel* GetMaterialModel();

        inline auto GetMesh() const noexcept
        { return mMesh; }

        inline bool HasMesh() const noexcept
        { return !!mMesh; }

        inline size_t GetDrawableCount() const noexcept
        { return mState.drawables.size(); }
        inline size_t GetMaterialCount() const noexcept
        { return mState.materials.size(); }

        inline glm::vec3 GetMinVector() const noexcept
        { return mMinValues; }
        inline glm::vec3 GetMaxVector() const noexcept
        { return mMaxValues; }

        const DrawableInfo& GetDrawable(size_t index) const noexcept
        { return mState.drawables[index]; }
        const MaterialInfo& GetMaterial(size_t index) const noexcept
        { return mState.materials[index]; }

        const MaterialInfo* FindMaterial(const AnyString& key) const noexcept;

        bool LoadModel(const app::AnyString& file);

        ModelImporter& operator=(const ModelImporter&) = delete;
    private:
        class DrawableModel;
        class MaterialModel;
        struct State {
            std::vector<DrawableInfo> drawables;
            std::vector<MaterialInfo> materials;
        } mState;
        std::shared_ptr<gfx::PolygonMeshClass> mMesh;
        std::unique_ptr<DrawableModel> mDrawableModel;
        std::unique_ptr<MaterialModel> mMaterialModel;

        glm::vec3 mMinValues = {0.0f, 0.0f, 0.0f};
        glm::vec3 mMaxValues = {0.0f, 0.0f, 0.0f};
    };

} // namespace