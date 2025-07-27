#pragma once

#include "containers/kraft_array.h"
#include "core/kraft_string.h"
#include "core/kraft_string_view.h"
#include "math/kraft_math.h"
#include "world/kraft_entity_types.h"

namespace kraft {

struct Material;

struct TransformComponent
{
    // You must call ComputeModelMatrix() after setting this!
    Vec3f Position = kraft::Vec3fZero;

    // You must call ComputeModelMatrix() after setting this!
    Vec3f Rotation = kraft::Vec3fZero;

    // You must call ComputeModelMatrix() after setting this!
    Vec3f Scale = kraft::Vec3fOne;
    Mat4f ModelMatrix = kraft::ScaleMatrix(Scale) * kraft::RotationMatrixFromEulerAngles(Rotation) * kraft::TranslationMatrix(Position);

    TransformComponent() = default;
    TransformComponent(Vec3f Position, Vec3f Rotation, Vec3f Scale) : Position(Position), Rotation(Rotation), Scale(Scale)
    {
        this->ComputeModelMatrix();
    }

    void ComputeModelMatrix()
    {
        // Since we have row-major matrices, the correct order of multiplication is SRT
        ModelMatrix = kraft::ScaleMatrix(Scale) * kraft::RotationMatrixFromEulerAngles(Rotation) * kraft::TranslationMatrix(Position);
        // ModelMatrix = kraft::TranslationMatrix(Position) * kraft::RotationMatrixFromEulerAngles(Rotation) * kraft::ScaleMatrix(Scale);
    }

    void SetTransform(Vec3f Position, Vec3f Rotation, Vec3f Scale)
    {
        this->Position = Position;
        this->Rotation = Rotation;
        this->Scale = Scale;

        this->ComputeModelMatrix();
    }

    void SetPosition(Vec3f Position)
    {
        this->Position = Position;
        this->ComputeModelMatrix();
    }

    void SetRotation(Vec3f Rotation)
    {
        this->Rotation = Rotation;
        this->ComputeModelMatrix();
    }

    void SetScale(Vec3f Scale)
    {
        this->Scale = Scale;
        this->ComputeModelMatrix();
    }

    void SetScale(float32 Scale)
    {
        this->Scale = kraft::Vec3f{ Scale, Scale, Scale };
        this->ComputeModelMatrix();
    }

    KRAFT_INLINE void Set(Mat4f matrix)
    {
        this->ModelMatrix = matrix;
        this->ModelMatrix.Decompose(this->Position, this->Rotation, this->Scale);
    }
};

struct RelationshipComponent
{
    EntityHandleT        Parent = EntityHandleT(-1);
    Array<EntityHandleT> Children;

    RelationshipComponent() = default;
    RelationshipComponent(EntityHandleT Parent) : Parent(Parent) {};
};

struct MetadataComponent
{
    String Name = "";

    MetadataComponent() = default;
    MetadataComponent(const String& Name) : Name(Name) {};
    MetadataComponent(StringView Name) : Name(Name.Buffer, Name.GetLengthInBytes()) {};
};

struct MeshComponent
{
    Material* MaterialInstance = nullptr;
    uint32    GeometryID = -1;

    MeshComponent() = default;
    MeshComponent(Material* MaterialInstance, uint32 GeometryID) : MaterialInstance(MaterialInstance), GeometryID(GeometryID) {};
};

struct LightComponent
{
    kraft::Vec4f LightColor = kraft::Vec4fOne;

    LightComponent() = default;
    LightComponent(kraft::Vec4f Color) : LightColor(Color) {};
};

}