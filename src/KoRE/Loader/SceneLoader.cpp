#include "KoRE/Loader/SceneLoader.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "KoRE/ResourceManager.h"
#include "KoRE/Loader/MeshLoader.h"
#include "KoRE/Components/Transform.h"
#include "KoRE/Components/Camera.h"
#include "KoRE/Components/LightComponent.h"
#include "KoRE/Components/Material.h"

kore::SceneLoader* kore::SceneLoader::getInstance() {
  static SceneLoader instance;
  return &instance;
}

kore::SceneLoader::SceneLoader()
                  : _nodecount(0),
                    _cameracount(0),
                    _meshcount(0) {
}

kore::SceneLoader::~SceneLoader() {
}

void kore::SceneLoader::loadScene(const std::string& szScenePath,
                                  SceneNode* parent) {
  _nodecount = _cameracount = _meshcount = 0;
  loadRessources(szScenePath);
  const aiScene* pAiScene =
    _aiImporter.ReadFile(szScenePath,
                         aiProcess_JoinIdenticalVertices 
                         | aiProcess_Triangulate
                         | aiProcess_CalcTangentSpace);

  if (!pAiScene) {
    Log::getInstance()
      ->write("[ERROR] Scene '%s' could not be loaded\n",
              szScenePath.c_str());
    return;
  }
  loadSceneGraph(pAiScene->mRootNode, parent, pAiScene, szScenePath);
  Log::getInstance()
    ->write("[DEBUG] Scene '%s' successfully loaded:\n"
            "\t %i meshes\n"
            "\t %i cameras\n"
            "\t %i nodes\n",
            szScenePath.c_str(),
            _meshcount,
            _cameracount,
            _nodecount);
}

void kore::SceneLoader::loadRessources(const std::string& szScenePath) {
  const aiScene* pAiScene =
    _aiImporter.ReadFile(szScenePath,
    aiProcess_JoinIdenticalVertices 
    | aiProcess_Triangulate
    | aiProcess_CalcTangentSpace);

  if (!pAiScene) {
    Log::getInstance()
      ->write("[ERROR] Scene '%s' could not be loaded\n",
              szScenePath.c_str());
    return;
  }
  
  if (pAiScene->HasMeshes()) {
    for (uint i = 0; i < pAiScene->mNumMeshes; ++i) {
      ResourceManager::getInstance()
        ->addMesh(szScenePath,
                  MeshLoader::getInstance()->loadMesh(pAiScene,i));
      _meshcount++;
    }
  }

  if (pAiScene->HasCameras()) {
    for (uint i = 0; i < pAiScene->mNumCameras; ++i) {
      const aiCamera* pAiCamera = pAiScene->mCameras[i];
      Camera* pCamera = new Camera;
      pCamera->setName(getCameraName(pAiCamera, i));
      float yFovDeg = glm::degrees(pAiCamera->mHorizontalFOV)
                     / pAiCamera->mAspect;
      pCamera->setProjectionPersp(yFovDeg,
                                  pAiCamera->mAspect,
                                  pAiCamera->mClipPlaneNear,
                                  pAiCamera->mClipPlaneFar);

      SceneManager::getInstance()->addCamera(szScenePath, pCamera);
      _cameracount++;
    }
  }

  if (pAiScene->HasLights()) {
    for (uint i = 0; i < pAiScene->mNumLights; ++i) {
      const aiLight* pAiLight = pAiScene->mLights[i];
      LightComponent* pLight = new LightComponent;
      pLight->setName(getLightName(pAiLight, i));
      
      pLight->_color = glm::vec3(pAiLight->mColorDiffuse.r,
                                 pAiLight->mColorDiffuse.g,
                                 pAiLight->mColorDiffuse.b);
      pLight->_intensity = glm::length(pLight->_color);
      pLight->_color = glm::normalize(pLight->_color);

      pLight->_falloffStart = 0.0f;
      pLight->_falloffEnd = 10.0f;  // TODO(dlazarek): find this info in the ai-light
      
      SceneManager::getInstance()->addLight(szScenePath, pLight);
      _lightcount++;
    }
  }


}

void kore::SceneLoader::loadSceneGraph(const aiNode* ainode,
                                       SceneNode* parentNode,
                                       const aiScene* aiscene,
                                       const std::string& szScenePath) {
    SceneNode* node = new SceneNode;
    node->getTransform()->setLocal(glmMatFromAiMat(ainode->mTransformation));
    node->_parent = parentNode;
    node->_dirty = true;
    node->_name = ainode->mName.C_Str();
    parentNode->_children.push_back(node);
    _nodecount++;

    // Load light if this node has one
    uint lightIndex = KORE_UINT_INVALID;
    for (uint i = 0; i < aiscene->mNumLights; ++i) {
      const aiLight* pAiLight = aiscene->mLights[i];
      std::string lightName = std::string(pAiLight->mName.C_Str());
      if (lightName == node->_name) {
        lightIndex = i;
        break;
      }
    }

    if (lightIndex != KORE_UINT_INVALID) {
      const aiLight* pAiLight = aiscene->mLights[lightIndex];
      std::string lightName = getLightName(pAiLight, lightIndex);
      LightComponent* pLight = SceneManager::getInstance()
                      ->getLight(szScenePath, lightName);
      if (pLight != NULL) {
        node->addComponent(pLight);
      }
    }

    // Determine if this node has a camera
    uint camIndex = KORE_UINT_INVALID;
    for (uint i = 0; i < aiscene->mNumCameras; ++i) {
      const aiCamera* pAiCam = aiscene->mCameras[i];
      std::string camName = std::string(pAiCam->mName.C_Str());
      if (camName == node->_name) {
        camIndex = i;
        break;
      }
    }

    if (camIndex != KORE_UINT_INVALID) {
      const aiCamera* pAiCam = aiscene->mCameras[camIndex];
      std::string camName = getCameraName(pAiCam, camIndex);
      Camera* pCamera = SceneManager::getInstance()
                                            ->getCamera(szScenePath, camName);
      if (pCamera != NULL) {
        node->addComponent(pCamera);
      }
    }

    
    // Load the first mesh as a component of this node.
    // Further meshes have to be loaded into duplicate nodes
    if (ainode->mNumMeshes > 0) {
      const aiMesh* aimesh = aiscene->mMeshes[ainode->mMeshes[0]];
      std::string meshName = MeshLoader::getInstance()
        ->getMeshName(aimesh, ainode->mMeshes[0]);
      Mesh* mesh = ResourceManager::getInstance()
        ->getMesh(szScenePath, meshName);
      MeshComponent* meshComponent = new MeshComponent;
      meshComponent->setMesh(mesh);
      node->addComponent(meshComponent);

      // Load the material for this mesh. Note that for every mesh, there is
      // a material in Assimp.
      Material* materialComponent = new Material;
      loadMaterialProperties(materialComponent,
                             aiscene->mMaterials[aimesh->mMaterialIndex]);
      node->addComponent(materialComponent);

    // Make additional copies for any more meshes
    for (uint iMesh = 1; iMesh < ainode->mNumMeshes; ++iMesh) {
      SceneNode* copyNode = new SceneNode;
      copyNode->_transform->setLocal(glmMatFromAiMat(ainode->mTransformation));
      copyNode->_parent = parentNode;
      copyNode->_dirty = true;
      parentNode->_children.push_back(copyNode);
      
      const aiMesh* aimesh = aiscene->mMeshes[ainode->mMeshes[iMesh]];
      std::string meshName = MeshLoader::getInstance()
        ->getMeshName(aimesh, ainode->mMeshes[iMesh]);

      Mesh* mesh = ResourceManager::getInstance()
        ->getMesh(szScenePath, meshName);

      MeshComponent* meshComponent = new MeshComponent;
      meshComponent->setMesh(mesh);
      copyNode->addComponent(meshComponent);

      // Load the material for this mesh. Note that for every mesh, there is
      // a material in Assimp.
      Material* materialComponent = new Material;
      loadMaterialProperties(materialComponent,
        aiscene->mMaterials[aimesh->mMaterialIndex]);
      node->addComponent(materialComponent);
    }
  }

  for (uint iChild = 0; iChild < ainode->mNumChildren; ++iChild) {
    loadSceneGraph(ainode->mChildren[iChild], node, aiscene, szScenePath);
  }
}

glm::mat4 kore::SceneLoader::glmMatFromAiMat(const aiMatrix4x4& aiMat) const {
  // Note: ai-matrix is row-major, but glm::mat4 is column-major
  return glm::mat4(aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
                   aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
                   aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
                   aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4);
}

std::string kore::SceneLoader::getCameraName(const aiCamera* paiCamera,
                                            const uint uSceneCameraIdx) {
  std::string camName = "";
  if (paiCamera->mName.length > 0) {
    camName = std::string(paiCamera->mName.C_Str());
  } else {
    char camNameBuf[100];
    sprintf(camNameBuf, "%i", uSceneCameraIdx);
    camName = std::string(&camNameBuf[0]);
    Log::getInstance()->write("[WARNING] Trying to load a camera without a"
                              "name. As a result, there will be no sceneNode"
                              "information for this camera.");
  }
  return camName;
}

std::string kore::SceneLoader::getLightName(const aiLight* pAiLight,
                                            const uint uSceneLightIndex) {
  std::string lightName = "";
  if (pAiLight->mName.length > 0) {
    lightName = std::string(pAiLight->mName.C_Str());
  } else {
    char lightNameBuf[100];
    sprintf(lightNameBuf, "%i", uSceneLightIndex);
    lightName = std::string(&lightNameBuf[0]);
    Log::getInstance()->write("[WARNING] Trying to load a light without a"
                              "name. As a result, there will be no sceneNode"
                              "information for this light.");
  }
  return lightName;
}

void kore::SceneLoader::
  loadMaterialProperties(Material* koreMat, const aiMaterial* aiMat) {
    // Note(dlazarek): Because of the somewhat ugly Assimp-api concerning
    // material-properties, currently the safest way is to process all material
    // constants one by one. Keep in mind that this list hast to be extended
    // if the assimp material-API changes.
    
    // Temp-vars to retrieve the values from assimp
    aiColor3D colValue;
    int intValue;
    float floatValue;

    // Diffuse color/reflectivity
    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, colValue) == AI_SUCCESS) {
      glm::vec3* pValue = new glm::vec3(colValue.r, colValue.g, colValue.b);
      koreMat->addValue("Diffuse Color", GL_FLOAT_VEC3, pValue);
    }

    //Specular
    if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, colValue) == AI_SUCCESS) {
      glm::vec3* pValue = new glm::vec3(colValue.r, colValue.g, colValue.b);
      koreMat->addValue("Specular Color", GL_FLOAT_VEC3, pValue);
    }

    // Ambient
    if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, colValue) == AI_SUCCESS) {
      glm::vec3* pValue = new glm::vec3(colValue.r, colValue.g, colValue.b);
      koreMat->addValue("Ambient Color", GL_FLOAT_VEC3, pValue);
    }

    // Emissive
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, colValue) == AI_SUCCESS) {
      glm::vec3* pValue = new glm::vec3(colValue.r, colValue.g, colValue.b);
      koreMat->addValue("Emissive Color", GL_FLOAT_VEC3, pValue);
    }

    // Transparent color
    if (aiMat->Get(AI_MATKEY_COLOR_TRANSPARENT, colValue) == AI_SUCCESS) {
      glm::vec3* pValue = new glm::vec3(colValue.r, colValue.g, colValue.b);
      koreMat->addValue("Transparent Color", GL_FLOAT_VEC3, pValue);
    }

    // Reflective color
    if (aiMat->Get(AI_MATKEY_COLOR_REFLECTIVE, colValue) == AI_SUCCESS) {
      glm::vec3* pValue = new glm::vec3(colValue.r, colValue.g, colValue.b);
      koreMat->addValue("Reflective Color", GL_FLOAT_VEC3, pValue);
    }

    // Enable Wireframe
    if (aiMat->Get(AI_MATKEY_ENABLE_WIREFRAME, intValue) == AI_SUCCESS) {
      int* pValue = new int(intValue);
      koreMat->addValue("Enable Wireframe", GL_INT, pValue);
    }

    // Twosided
    if (aiMat->Get(AI_MATKEY_TWOSIDED, intValue) == AI_SUCCESS) {
      int* pValue = new int(intValue);
      koreMat->addValue("Twosided", GL_INT, pValue);
    }

    // Opacity
    if (aiMat->Get(AI_MATKEY_OPACITY, floatValue) == AI_SUCCESS) {
      float* pValue = new float(floatValue);
      koreMat->addValue("Opacity", GL_FLOAT, pValue);
    }

    // Shininess
    if (aiMat->Get(AI_MATKEY_SHININESS, floatValue) == AI_SUCCESS) {
      float* pValue = new float(floatValue);
      koreMat->addValue("Shininess", GL_FLOAT, pValue);
    }

    // Shininess-Strength
    if (aiMat->Get(AI_MATKEY_SHININESS_STRENGTH, floatValue) == AI_SUCCESS) {
      float* pValue = new float(floatValue);
      koreMat->addValue("Shininess-Strength", GL_FLOAT, pValue);
    }

    // Refraction index
    if (aiMat->Get(AI_MATKEY_REFRACTI, floatValue) == AI_SUCCESS) {
      float* pValue = new float(floatValue);
      koreMat->addValue("Refraction index", GL_FLOAT, pValue);
    }

    // Bump-scaling
    if (aiMat->Get(AI_MATKEY_BUMPSCALING, floatValue) == AI_SUCCESS) {
      float* pValue = new float(floatValue);
      koreMat->addValue("Bump strength", GL_FLOAT, pValue);
    }
}

void kore::SceneLoader::loadMaterialTextures(TexturesComponent* texComponent,
                                             const aiMaterial* aiMat) {
    for (uint i = 0; i < aiMat->mNumProperties; ++i) {
      const aiMaterialProperty* aiMatProp = aiMat->mProperties[i];
      if (aiMatProp->mSemantic == aiTextureType_NONE) {
        continue;  // skip non-texture properties
      }
    }
}
