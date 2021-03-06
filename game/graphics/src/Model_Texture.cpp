#include <Model_Texture.hpp>
#include <Shaders.hpp>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GraphicsErrors.hpp>
#include <Coplien.hpp>

Model_Texture::Model_Texture()
{
    this->_isLoaded = false;
}

Model_Texture::Model_Texture(const Model_Texture & src)
{
    *this = src;
}

Model_Texture::Model_Texture(char *path)
{
    loadModel(path);
}

Model_Texture::~Model_Texture()
{
    for (size_t i = 0; i < this->_meshes.size(); i++)
    {
        glDeleteVertexArrays(1, &this->_meshes[i]->VAO);
        glDeleteBuffers(1, &this->_meshes[i]->VBO);
        glDeleteBuffers(1, &this->_meshes[i]->EBO);
        delete this->_meshes[i];
    }
}

Model_Texture & Model_Texture::operator=(const Model_Texture & src)
{
    if (this != &src)
    {
        copy(this->_isLoaded, src._isLoaded);
        copy(this->_meshes, src._meshes);
        copy(this->_textureLoaded, src._textureLoaded);
        copy(this->_directory, src._directory);
    }
    return *this;
}

bool Model_Texture::IsLoaded() const
{
    return this->_isLoaded;
}

void Model_Texture::Draw(Shaders & shader)
{
    if (this->_isLoaded)
    {
        for (size_t i = 0; i < this->_meshes.size(); i++)
            this->_meshes[i]->Draw(shader);
    }
    else
        throw GraphicsErrors::TextureNotLoaded(this->_directory);
}

void Model_Texture::loadModel(std::string path)
{
    Assimp::Importer import;
    const aiScene * scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
    {
        this->_isLoaded = false;
        throw GraphicsErrors::AssimpError(import.GetErrorString());
    }
    this->_directory = path.substr(0, path.find_last_of('/'));

    processNode(scene->mRootNode, scene);
    this->_isLoaded = true;
    getBox();
}

void Model_Texture::processNode(aiNode *node, const aiScene *scene)
{
    // process all the node's meshes (if any)
    for(unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]]; 
        processMesh(mesh, scene);			
    }
    // then do the same for each of its children
    for(unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}

void Model_Texture::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName, std::vector<Texture> & textures)
{
    for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
        bool skip = false;
        for(unsigned int j = 0; j < _textureLoaded.size(); j++)
        {
            if(std::strcmp(_textureLoaded[j]._path.data(), str.C_Str()) == 0)
            {
                textures.push_back(_textureLoaded[j]);
                skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                break;
            }
        }
        if(!skip)
        {   // if texture hasn't been loaded already, load it
            Texture texture;
            texture._id = TextureFromFile(str.C_Str(), this->_directory);
            texture._type = typeName;
            texture._path = str.C_Str();
            textures.push_back(texture);
            _textureLoaded.push_back(texture);  // store it as texture loaded for entire Model_Texture, to ensure we won't unnecesery load duplicate textures.
        }
    }
}

void Model_Texture::processMesh(aiMesh *mesh, const aiScene *scene)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    // Walk through each of the mesh's vertices
    for(unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        glm::vec3 vector; 

        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex._position = vector;
        // normals
        vector.x = mesh->mNormals[i].x;
        vector.y = mesh->mNormals[i].y;
        vector.z = mesh->mNormals[i].z;
        vertex._normal = vector;
        // texture coordinates
        if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            glm::vec2 vec;
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
            // use Model_Texture where a vertex can have multiple texture coordinates so we always take the first set (0).
            vec.x = mesh->mTextureCoords[0][i].x; 
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex._texCoords = vec;
        }
        else
            vertex._texCoords = glm::vec2(0.0f, 0.0f);
        // tangent
        vector.x = mesh->mTangents[i].x;
        vector.y = mesh->mTangents[i].y;
        vector.z = mesh->mTangents[i].z;
        vertex._tangent = vector;
        // bitangent
        vector.x = mesh->mBitangents[i].x;
        vector.y = mesh->mBitangents[i].y;
        vector.z = mesh->mBitangents[i].z;

        vertex._bitangent = vector;
        vertices.push_back(vertex);
    }
    // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for(unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        // retrieve all indices of the face and store them in the indices vector
        for(unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }
    // process materials
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    
    
    std::vector<Texture> diffuseMaps;
    loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", diffuseMaps);
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    std::vector<Texture> specularMaps;
    loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", specularMaps);
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::vector<Texture> normalMaps;
    loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", normalMaps);
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::vector<Texture> heightMaps;
    loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height", heightMaps);
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
    
    // return a mesh object created from the extracted mesh data
    this->_meshes.push_back(new Mesh(vertices, indices, textures));
}

unsigned int Model_Texture::TextureFromFile(const char *path, const std::string &directory)
{
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width = 0;
    int height = 0;
    int nrComponents = 0;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = 0;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        this->_isLoaded = false;
        stbi_image_free(data);
        throw GraphicsErrors::TextureFailed(path);
    }

    return textureID;
}

void checkValues(float & num1, float & num2, float value)
{
    if (value > num1)
        num1 = value;
    else if (value < num2)
        num2 = value;
}

void Model_Texture::getBox()
{
    glm::vec3 pos = this->_meshes[0]->_vertices[0]._position;
    this->_boundingBox.x1 = pos.x;
    this->_boundingBox.x2 = pos.x;
    this->_boundingBox.y1 = pos.y;
    this->_boundingBox.y2 = pos.y;
    this->_boundingBox.z1 = pos.z;
    this->_boundingBox.z2 = pos.z;
    for (size_t i = 0; i < this->_meshes.size(); i++)
    {
        for (size_t j = 0; j < this->_meshes[i]->_vertices.size(); j++)
        {
            checkValues(_boundingBox.x1, _boundingBox.x2, this->_meshes[i]->_vertices[j]._position.x);
            checkValues(_boundingBox.y1, _boundingBox.y2, this->_meshes[i]->_vertices[j]._position.y);
            checkValues(_boundingBox.z1, _boundingBox.z2, this->_meshes[i]->_vertices[j]._position.z);
        }
    }
}