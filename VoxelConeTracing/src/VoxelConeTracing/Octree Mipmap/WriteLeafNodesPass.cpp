/*
 Copyright (c) 2012 The VCT Project

  This file is part of VoxelConeTracing and is an implementation of
  "Interactive Indirect Illumination Using Voxel Cone Tracing" by Crassin et al

  VoxelConeTracing is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VoxelConeTracing is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with VoxelConeTracing.  If not, see <http://www.gnu.org/licenses/>.
*/

/*!
* \author Dominik Lazarek (dominik.lazarek@gmail.com)
* \author Andreas Weinmann (andy.weinmann@gmail.com)
*/

#include "VoxelConeTracing/Octree Mipmap/WriteLeafNodesPass.h"

#include "KoRE\RenderManager.h"
#include "KoRE\ResourceManager.h"

#include "Kore\Operations\Operations.h"

WriteLeafNodesPass::~WriteLeafNodesPass(void) {
}

WriteLeafNodesPass::
  WriteLeafNodesPass(VCTscene* vctScene,
                     kore::EOperationExecutionType executionType) {
  using namespace kore;
  
  this->setExecutionType(executionType);

  _vctScene = vctScene;
  _renderMgr = RenderManager::getInstance();
  _sceneMgr = SceneManager::getInstance();
  _resMgr = ResourceManager::getInstance();

  kore::ShaderProgram* shp = new kore::ShaderProgram;
  this->setShaderProgram(shp);
  shp->setName("OctreeWrite shader");
  shp->loadShader("./assets/shader/OctreeWriteLeafs.shader",
                 GL_VERTEX_SHADER);
  shp->init();
  
  
  // Launch a thread for every node up to the max level
  addStartupOperation(
    new kore::BindBuffer(GL_DRAW_INDIRECT_BUFFER,
        _vctScene->getVoxelFragList()->getFragListIndCmdBuf()->getBufferHandle()));

  addStartupOperation(new BindUniform(vctScene->getNodePool()->getShdNumLevels(),
                                              shp->getUniform("numLevels")));

  addStartupOperation(new BindImageTexture(
    vctScene->getVoxelFragList()->getShdVoxelFragList(VOXELATT_POSITION),
    shp->getUniform("voxelFragList_pos")));

  addStartupOperation(new BindImageTexture(
    vctScene->getVoxelFragList()->getShdVoxelFragList(VOXELATT_COLOR),
    shp->getUniform("voxelFragList_color")));

  addStartupOperation(new BindImageTexture(
    vctScene->getNodePool()->getShdNodePool(NEXT),
    shp->getUniform("nodePool_next")));

  addStartupOperation(new BindImageTexture(
    vctScene->getNodePool()->getShdNodePool(COLOR),
    shp->getUniform("nodePool_color")));

  addStartupOperation(new BindUniform(
    vctScene->getShdVoxelGridResolution(),
    shp->getUniform("voxelGridResolution")));

  addStartupOperation(
    new kore::DrawIndirectOp(GL_POINTS, 0));

  addFinishOperation(new MemoryBarrierOp(GL_ALL_BARRIER_BITS));
}
