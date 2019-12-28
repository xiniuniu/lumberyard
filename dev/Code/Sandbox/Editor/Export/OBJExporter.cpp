/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"

#include "OBJExporter.h"



const char* COBJExporter::GetExtension() const
{
    return "obj";
}


const char* COBJExporter::GetShortDescription() const
{
    return "Object files";
}


bool COBJExporter::ExportToFile(const char* filename, const Export::IData* pExportData)
{
    CLogFile::FormatLine("Exporting OBJ file to '%s'", filename);

    FILE* hFile = nullptr;
    azfopen(&hFile, filename, "w");
    if (!hFile)
    {
        CLogFile::FormatLine("Error while opening file '%s'!", filename);
        assert(hFile);
        return false;
    }

    // Write header
    fprintf(hFile, "# Object file exported by Sandbox\n");
    fprintf(hFile, "# Attention: while import to 3DS Max Unify checkbox for normals must be unchecked.\n");
    fprintf(hFile, "#\n");

    // Create MTL library filename
    QString materialFilename = filename;
    materialFilename = Path::ReplaceExtension(materialFilename, "mtl");
    materialFilename = Path::GetFile(materialFilename);

    // Write material library import statement
    fprintf(hFile, "mtllib %s\n", materialFilename.toUtf8().data());
    fprintf(hFile, "#\n");

    int numObjects = pExportData->GetObjectCount();
    for (int i = 0; i < numObjects; ++i)
    {
        const Export::Object* pObj = pExportData->GetObject(i);

        Vec3 pos(pObj->pos.x, pObj->pos.y, pObj->pos.z);
        Quat rot(pObj->rot.w, pObj->rot.v.x, pObj->rot.v.y, pObj->rot.v.z);
        Vec3 scale(pObj->scale.x, pObj->scale.y, pObj->scale.z);

        Matrix34 tm = Matrix33::CreateScale(scale) * Matrix34(rot);
        tm.SetTranslation(pos);

        int nParent = pObj->nParent;
        while (nParent >= 0 && nParent < pExportData->GetObjectCount())
        {
            const Export::Object* pParentObj = pExportData->GetObject(nParent);
            assert(NULL != pParentObj);

            Vec3 pos(pParentObj->pos.x, pParentObj->pos.y, pParentObj->pos.z);
            Quat rot(pParentObj->rot.w, pParentObj->rot.v.x, pParentObj->rot.v.y, pParentObj->rot.v.z);
            Vec3 scale(pParentObj->scale.x, pParentObj->scale.y, pParentObj->scale.z);

            Matrix34 parentTm = Matrix33::CreateScale(scale) * Matrix34(rot);
            parentTm.SetScale(scale);
            parentTm.SetTranslation(pos);

            tm = tm * parentTm;
            nParent = pParentObj->nParent;
        }

        fprintf(hFile, "g %s\n", pObj->name); //For XSI
        fprintf(hFile, "# object %s\n", pObj->name);
        fprintf(hFile, "#\n");

        int numVertices = pObj->GetVertexCount();
        const Export::Vector3D* pVerts = pObj->GetVertexBuffer();

        for (int i = 0; i < numVertices; ++i)
        {
            const Export::Vector3D& vertex = pVerts[i];

            Vec3 vec(vertex.x, vertex.y, vertex.z);
            vec = tm.TransformPoint(vec);

            fprintf(hFile, "v %s %s %s\n", TrimFloat(vec.x), TrimFloat(vec.y), TrimFloat(vec.z));
        }
        fprintf(hFile, "# %i vertices\n\n", numVertices);

        // Write object texture coordinates
        int numTexCoords = pObj->GetTexCoordCount();
        const Export::UV* pTexCoord = pObj->GetTexCoordBuffer();
        for (int i = 0; i < numTexCoords; ++i)
        {
            const Export::UV& textCoord = pTexCoord[i];
            fprintf(hFile, "vt %s %s 0\n", TrimFloat(textCoord.u), TrimFloat(textCoord.v));
        }
        fprintf(hFile, "# %i texture vertices\n\n", numTexCoords);

        // Write object normals
        int numNormals = pObj->GetNormalCount();
        const Export::Vector3D* pNormals = pObj->GetNormalBuffer();
        for (int i = 0; i < numNormals; ++i)
        {
            const Export::Vector3D& normal = pNormals[i];
            fprintf(hFile, "vn %s %s %s\n", TrimFloat(normal.x), TrimFloat(normal.y), TrimFloat(normal.z));
        }
        fprintf(hFile, "# %i vertex normals\n\n", numNormals);


        // Write submeshes
        int numMeshes = pObj->GetMeshCount();
        for (int j = 0; j < numMeshes; ++j)
        {
            const Export::Mesh* pMesh = pObj->GetMesh(j);
            if (pMesh->material.name[0] != '\0')
            {
                fprintf(hFile, "usemtl %s\n", pMesh->material.name);
            }

            // TODO: if it's a smoth group fix it to bit difference
            fprintf(hFile, "s %d\n", j);

            // Write all faces, convert the indices to one based indices
            int numFaces = pMesh->GetFaceCount();
            const Export::Face* pFaceBuf = pMesh->GetFaceBuffer();
            for (int i = 0; i < numFaces; ++i)
            {
                const Export::Face& face = pFaceBuf[i];
                fprintf(hFile, "f %i/%i/%i %i/%i/%i %i/%i/%i\n",
                    face.idx[0] - numVertices, face.idx[0] - numTexCoords, face.idx[0] - numNormals,
                    face.idx[1] - numVertices, face.idx[1] - numTexCoords, face.idx[1] - numNormals,
                    face.idx[2] - numVertices, face.idx[2] - numTexCoords, face.idx[2] - numNormals);
            }
            fprintf(hFile, "# %i faces\n\n", numFaces);
        }
    }

    fprintf(hFile, "g\n");
    fclose(hFile);


    // Export Material
    materialFilename = Path::ReplaceExtension(filename, "mtl");

    // Open the material file
    hFile = nullptr;
    azfopen(&hFile, materialFilename.toUtf8().data(), "w");
    if (!hFile)
    {
        CLogFile::FormatLine("Error while opening file '%s'!", materialFilename.toUtf8().data());
        assert(hFile);
        return false;
    }

    // Write header
    fprintf(hFile, "# Material file exported by Sandbox\n\n");

    for (int i = 0; i < numObjects; ++i)
    {
        const Export::Object* pObj = pExportData->GetObject(i);

        int numMeshes = pObj->GetMeshCount();
        for (int j = 0; j < numMeshes; j++)
        {
            const Export::Mesh* pMesh = pObj->GetMesh(j);
            if (pMesh->material.name[0] != '\0')
            {
                // Write material
                const Export::Material& mtl = pMesh->material;
                fprintf(hFile, "newmtl %s\n", mtl.name);
                fprintf(hFile, "Ka %s %s %s\n", TrimFloat(mtl.diffuse.r), TrimFloat(mtl.diffuse.g), TrimFloat(mtl.diffuse.b));
                fprintf(hFile, "Kd %s %s %s\n", TrimFloat(mtl.diffuse.r), TrimFloat(mtl.diffuse.g), TrimFloat(mtl.diffuse.b));
                fprintf(hFile, "Ks %s %s %s\n", TrimFloat(mtl.specular.r), TrimFloat(mtl.specular.g), TrimFloat(mtl.specular.b));
                fprintf(hFile, "d %s\n", TrimFloat(1.0f - mtl.opacity));
                fprintf(hFile, "Tr %s\n", TrimFloat(1.0f - mtl.opacity));
                fprintf(hFile, "Ns %s\n", TrimFloat(mtl.smoothness));
                if (strlen(mtl.mapDiffuse))
                {
                    fprintf(hFile, "map_Kd %s\n", MakeRelativePath(filename, mtl.mapDiffuse).toUtf8().constData());
                }
                if (strlen(mtl.mapSpecular))
                {
                    fprintf(hFile, "map_Ns %s\n", MakeRelativePath(filename, mtl.mapSpecular).toUtf8().constData());
                }
                if (strlen(mtl.mapOpacity))
                {
                    fprintf(hFile, "map_d %s\n", MakeRelativePath(filename, mtl.mapOpacity).toUtf8().constData());
                }
                if (strlen(mtl.mapNormals))
                {
                    fprintf(hFile, "bump %s\n", MakeRelativePath(filename, mtl.mapNormals).toUtf8().constData());
                }
                if (strlen(mtl.mapDecal))
                {
                    fprintf(hFile, "decal %s\n", MakeRelativePath(filename, mtl.mapDecal).toUtf8().constData());
                }
                if (strlen(mtl.mapDisplacement))
                {
                    fprintf(hFile, "disp %s\n", MakeRelativePath(filename, mtl.mapDisplacement).toUtf8().constData());
                }
                fprintf(hFile, "\n");
            }
        }
    }

    fclose(hFile);

    return true;
}


QString COBJExporter::MakeRelativePath(const char* pMainFileName, const char* pFileName) const
{
    const char* ch = strrchr(pMainFileName, '\\');
    if (ch)
    {
        if (strlen(pFileName) > ch - pMainFileName && !_strnicmp(pMainFileName, pFileName, ch - pMainFileName))
        {
            return QString(pFileName + (ch - pMainFileName) + 1);
        }
    }

    return QString(pFileName);
}


const char* COBJExporter::TrimFloat(float fValue) const
{
    // Convert a float into a string representation and remove all
    // uneccessary zeroes and the decimal dot if it is not needed
    const int nMaxAccessInTime = 4;

    static char ppBufs[nMaxAccessInTime][16];
    static int nCurBuf = 0;

    if (nCurBuf >= nMaxAccessInTime)
    {
        nCurBuf = 0;
    }

    char* pBuf = ppBufs[nCurBuf];
    size_t bufSize = sizeof(ppBufs[nCurBuf]);
    ++nCurBuf;
    sprintf_s(pBuf, bufSize, "%f", fValue);

    for (int i = strlen(pBuf) - 1; i > 0; --i)
    {
        if (pBuf[i] == '0')
        {
            pBuf[i] = 0;
        }
        else if (pBuf[i] == '.')
        {
            pBuf[i] = 0;
            break;
        }
        else
        {
            break;
        }
    }

    return pBuf;
}



void COBJExporter::Release()
{
    delete this;
}