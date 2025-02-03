#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <assimp/scene.h>

#include "Mesh.h"
#include "Device.h"

class MeshModel
{
public:
	MeshModel();
	MeshModel(std::vector<Mesh> newMeshList);

	size_t getMeshCount();
	Mesh* getMesh(size_t index);

	Model getModel();
	void setModel(glm::mat4 m);

	void destroyMeshModel();

	static std::vector<std::string> loadMaterials(const aiScene* scene);
	static std::vector<Mesh> LoadNode(Device* device,aiNode* node, const aiScene* scene, std::vector<int> matToTex);
	static Mesh loadMesh(Device* device, aiMesh* mesh, const aiScene* scene, std::vector<int> matToTex);

	std::vector<std::string> getTextures() { return textures; }

	~MeshModel();
private:
	std::vector<Mesh> meshList;
	Model model;

	std::vector<std::string> textures;
};

