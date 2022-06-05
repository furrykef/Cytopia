#include <cstdlib>
#include "MapNode.hxx"

#include "LOG.hxx"
#include "../map/MapLayers.hxx"
#include "GameStates.hxx"
#include "Settings.hxx"
#include "MapFunctions.hxx"

MapNode::MapNode(Point isoCoordinates, const std::string &terrainID, const std::string &tileID)
    : m_isoCoordinates(isoCoordinates), m_originCoordinates(isoCoordinates), m_sprite{std::make_unique<Sprite>(m_isoCoordinates)},
      m_autotileOrientation(LAYERS_COUNT, TileOrientation::TILE_DEFAULT_ORIENTATION),
      m_mapNodeData{std::vector(LAYERS_COUNT, MapNodeData{"", nullptr, 0, m_isoCoordinates, TileMap::DEFAULT})},
      m_autotileBitmask(LAYERS_COUNT)
{
  setTileID(terrainID, isoCoordinates);
  if (!tileID.empty()) // in case tileID is not supplied skip it
  {
    setTileID(tileID, isoCoordinates);
  }
  // always add blueprint tiles too when creating the node
  setTileID("terrain_blueprint", isoCoordinates);
  const Layer layer = TileManager::instance().getTileLayer(tileID);
  updateTexture(layer);
}

bool MapNode::changeHeight(const bool higher)
{
  constexpr int minHeight = 0;
  auto &height = m_isoCoordinates.height;

  if ((higher && (height < maxHeight)) || (!higher && (height > minHeight)))
  {
    higher ? ++height : --height;
    m_sprite->isoCoordinates = m_isoCoordinates;
    return true;
  }

  return false;
}

void MapNode::render() const { m_sprite->render(); }

void MapNode::setBitmask(unsigned char elevationBitmask, std::vector<uint8_t> autotileBitmask)
{
  m_elevationBitmask = elevationBitmask;
  m_autotileBitmask = autotileBitmask;
  updateTexture();
}

void MapNode::setTileID(const std::string &tileID, const Point &origCornerPoint)
{
  //TODO: don't set tile id if not origin, extra function
  // set other nodes to invis.
  // if other node is not origin, return tileID of origin
  // TODO: Reset everything on demolish
  // TODO: Find out what's missing for other things to work
  // TODO: remove origincornerpoint stuff
  TileData *tileData = TileManager::instance().getTileData(tileID);
  if (tileData && !tileID.empty())
  {
    m_originCoordinates = origCornerPoint;

    std::vector<Point> targetCoordinates = TileManager::instance().getTargetCoordsOfTileID(origCornerPoint, tileID);
    if (targetCoordinates.size() > 1 && m_isoCoordinates == origCornerPoint)
    { // multibuilding placed on this node
      // int minZ = m_isoCoordinates.z;
      // LOG(LOG_INFO) << "old z " << minZ;
      int minY = 0;
      // set origin corner node
      // for (auto coord : targetCoordinates)
      // {
      //   if (coord.x == origCornerPoint.x)
      //   {
      //     minZ = std::min(minZ, MapFunctions::instance().getMapNode(coord).getCoordinates().z);
      //     LOG(LOG_INFO) << "Setting new z " << minZ;
      //   }
      // }
      for (auto coord : targetCoordinates)
      {

        if (coord == origCornerPoint)
        {
          // m_originCoordinates = origCornerPoint;
          // LOG(LOG_INFO) << "i'm the origin coordinate";
        }
        else
        {
          LOG(LOG_ERROR) << "it's mjultile";
          // m_isOriginNode = false;
          m_multiTileCoords.push_back(coord);
          // m_multiTileNodes.push_back(&MapFunctions::instance().getMapNode(coord));
          MapFunctions::instance().getMapNode(coord).getSprite()->setRenderFlag(Layer::BUILDINGS, false);
          // MapFunctions::instance().getMapNode(coord).setRenderFlag(Layer::TERRAIN, false);
          MapFunctions::instance().getMapNode(coord).updateTexture(Layer::BUILDINGS);
          // MapFunctions::instance().getMapNode(coord).updateTexture(Layer::TERRAIN);
          // MapFunctions::instance().getMapNode(coord).setTileID(tileID, origCornerPoint);
          MapFunctions::instance().getMapNode(coord).setOriginCoordinate(origCornerPoint);
          // MapFunctions::instance().getMapNode(coord).setTileID("terrain_basalt", origCornerPoint);
          // MapFunctions::instance().getMapNode(coord).setZIndex(minZ);

          // LOG(LOG_INFO) << "i'm a multinode";
        }
      }
    }
    const Layer layer = TileManager::instance().getTileLayer(tileID);
    switch (layer)
    {
    case Layer::ZONE:
      this->setNodeTransparency(Settings::instance().zoneLayerTransparency, Layer::ZONE);
      break;
    case Layer::WATER:
      demolishLayer(Layer::ROAD);
      //TODO: we need to modify neighbors TileTypes to Shore.
      // no break on purpose.
    case Layer::ROAD:
      // in case it's allowed then maybe a Tree Tile already exist, so we remove it.
      demolishLayer(Layer::FLORA);
    case Layer::POWERLINES:
      break;
    case Layer::BUILDINGS:
      if (tileData->tileType != +TileType::FLORA)
      {
        this->setNodeTransparency(0.6, Layer::BLUEPRINT);
      }
      m_sprite->setRenderFlag(Layer::ZONE, false);
      break;
    default:
      break;
    }

    m_mapNodeData[layer].origCornerPoint = origCornerPoint;
    m_previousTileID = m_mapNodeData[layer].tileID;
    m_mapNodeData[layer].tileData = tileData;
    m_mapNodeData[layer].tileID = tileID;

    // Determine if the tile should have a random rotation or not.
    if (m_mapNodeData[layer].tileData->tiles.pickRandomTile && m_mapNodeData[layer].tileData->tiles.count > 1)
    {
      /** set tileIndex to a rand between 1 and count, this will be the displayed image of the entire tileset
      * if this tile has ordered frames, like roads then pickRandomTile must be set to 0.
      **/
      m_mapNodeData[layer].tileIndex = rand() % m_mapNodeData[layer].tileData->tiles.count;
    }
    else
    {
      /** must be reset to 0 otherwise overwritting tiles would keep the old
      * tile's tileIndex which creates problems if it's supposed to be 0
      **/
      m_mapNodeData[layer].tileIndex = 0;
    }
    updateTexture(layer);
  }
}

Layer MapNode::getTopMostActiveLayer() const
{
  for (auto currentLayer : layersInActiveOrder)
  {
    if (MapLayers::isLayerActive(currentLayer) && isLayerOccupied(currentLayer))
    {
      return currentLayer;
    }
  }
  return Layer::NONE;
}

void MapNode::setNodeTransparency(const float transparencyFactor, const Layer &layer) const
{
  // TODO refactoring: Consider replacing magic number (255) with constexpr.
  unsigned char alpha = (1 - transparencyFactor) * 255;
  m_sprite->setSpriteTranparencyFactor(layer, alpha);
}

bool MapNode::isPlacableOnSlope(const std::string &tileID) const
{
  TileData *tileData = TileManager::instance().getTileData(tileID);
  if (tileData && tileData->tileType == +TileType::ZONE)
  {
    // zones are allowed to pass slopes.
    return true;
  }
  if (tileData && m_elevationOrientation != TileSlopes::DEFAULT_ORIENTATION)
  {
    // we need to check the terrain layer for it's orientation so we can calculate the resulting x offset in the spritesheet.
    const int clipRectX = tileData->slopeTiles.clippingWidth * static_cast<int>(m_autotileOrientation[Layer::TERRAIN]);
    // while loading game, m_previousTileID will be equal to "terrain" for terrin tiles while it's empty "" when starting new game.
    // so the check here on m_previousTileID is needed both (temporary), empty and "terrain", this will be fixed in new PR.
    if (clipRectX >= static_cast<int>(tileData->slopeTiles.count) * tileData->slopeTiles.clippingWidth &&
        (m_previousTileID.empty() || m_previousTileID == "terrain"))
    {
      return false;
    }
  }
  return true;
}

bool MapNode::isPlacementAllowed(const std::string &newTileID) const
{
  TileData *tileData = TileManager::instance().getTileData(newTileID);

  if (tileData)
  {
    const Layer layer = TileManager::instance().getTileLayer(newTileID);
    // layer specific checks:
    switch (layer)
    {
    case Layer::ZONE:
      if (isLayerOccupied(Layer::WATER))
      {
        return tileData->placeOnWater;
      }
      return true;
    case Layer::POWERLINES:
      if (isLayerOccupied(Layer::POWERLINES) ||
          std::any_of(layersPowerlinesCanCross.begin(), layersPowerlinesCanCross.end(),
                      [this](const Layer &_layer) { return this->isLayerOccupied(_layer); }))
      { // powerlines can be placed over each other and over other low terrains
        return true;
      }
    case Layer::ROAD:
      if ((isLayerOccupied(Layer::BUILDINGS) && getTileData(Layer::BUILDINGS)->category != "Flora") ||
          isLayerOccupied(Layer::WATER) || !isPlacableOnSlope(newTileID))
      { // roads cannot be placed:
        // - on buildings that are not category flora.
        // - on water
        // - on slopetiles that don't have a tileID
        return false;
      }
      return true;
    case Layer::GROUND_DECORATION:
      if (isLayerOccupied(Layer::GROUND_DECORATION) || isLayerOccupied(Layer::BUILDINGS))
      { // allow placement of ground decoration on existing ground decoration and on buildings.
        return true;
      }
      break;
    case Layer::FLORA:
      if (isLayerOccupied(Layer::FLORA) && getTileData(Layer::FLORA)->isOverPlacable)
      { // flora with overplacable flag
        return true;
      }
      if (isLayerOccupied(Layer::ROAD) || isLayerOccupied(Layer::BUILDINGS) || isLayerOccupied(Layer::POWERLINES))
      { // don't allow placement of flora on buildings or roads
        return false;
      }
      break;
    case Layer::BUILDINGS:
    {
      if (isLayerOccupied(Layer::BUILDINGS) && getTileData(Layer::BUILDINGS)->isOverPlacable)
      { // buildings with overplacable flag
        return true;
      }
      if (isLayerOccupied(Layer::ROAD))
      { // buildings cannot be placed on roads
        return false;
      }
      break;
    }
    default:
      break;
    }

    // checks for all layers:
    if (isLayerOccupied(Layer::WATER))
    {
      if (tileData->tileType != +TileType::WATER && !tileData->placeOnWater)
      // Disallow placement on water for tiles that are:
      // not of tiletype water
      // not flag placeOnWater enabled
      {
        return false;
      }
    }
    else // not water
    {
      if (!tileData->placeOnGround)
      // Disallow placement on ground (meaning a tile that is not water) for tiles that have:
      // not flag placeOnGround enabled
      {
        return false;
      }
    }

    if (!isPlacableOnSlope(newTileID))
    { // Check if a tile has slope frames and therefore can be placed on a node with a slope
      return false;
    }

    if (tileData->tileType == +TileType::UNDERGROUND)
    { // Underground tiletype (pipes, metro tunnels, ... ) can overplace each other
      return true;
    }

    if (m_mapNodeData[layer].tileID.empty())
    { // of course allow placement on empty tiles
      return true;
    }
  }
  // every case that is not handled is false
  return false;
}

void MapNode::updateTexture(const Layer &layer)
{
  SDL_Rect clipRect{0, 0, 0, 0};
  //TODO: Refactor this
  m_elevationOrientation = TileManager::instance().calculateSlopeOrientation(m_elevationBitmask);
  std::vector<Layer> layersToGoOver;
  if (layer != Layer::NONE)
  {
    // in case this is not the default value (which is NONE), we need to update only 1 layer.
    layersToGoOver.push_back(layer);
  }
  else
  {
    layersToGoOver.insert(layersToGoOver.begin(), std::begin(allLayersOrdered), std::end(allLayersOrdered));
  }
  for (auto currentLayer : layersToGoOver)
  {
    const TileData *tileData = getTileData(currentLayer);
    if (tileData)
    {
      size_t spriteCount = 1;
      m_mapNodeData[currentLayer].tileMap = TileMap::DEFAULT;
      if (m_elevationOrientation == TileSlopes::DEFAULT_ORIENTATION)
      {
        if (tileData->tileType == +TileType::WATER || tileData->tileType == +TileType::TERRAIN ||
            tileData->tileType == +TileType::BLUEPRINT)
        {
          m_autotileOrientation[currentLayer] = TileManager::instance().calculateTileOrientation(m_autotileBitmask[currentLayer]);
          m_mapNodeData[currentLayer].tileMap = TileMap::DEFAULT;
          if (currentLayer == Layer::TERRAIN && m_autotileOrientation[currentLayer] != TileOrientation::TILE_DEFAULT_ORIENTATION)
          {
            m_mapNodeData[Layer::TERRAIN].tileMap = TileMap::SHORE;
            // for shore tiles, we need to reset the tileIndex to 0, else a random tile would be picked. This is a little bit hacky.
            m_mapNodeData[Layer::TERRAIN].tileIndex = 0;
          }
        }
        // if the node can autotile, calculate it's tile orientation
        else if (TileManager::instance().isTileIDAutoTile(getTileID(currentLayer)))
        {
          m_autotileOrientation[currentLayer] = TileManager::instance().calculateTileOrientation(m_autotileBitmask[currentLayer]);
        }
      }
      else if (m_elevationOrientation >= TileSlopes::N && m_elevationOrientation <= TileSlopes::BETWEEN)
      {
        if (tileData->slopeTiles.fileName.empty())
        {
          m_mapNodeData[currentLayer].tileMap = TileMap::DEFAULT;
          m_autotileOrientation[currentLayer] = TileOrientation::TILE_DEFAULT_ORIENTATION;
        }
        else
        {
          m_mapNodeData[currentLayer].tileMap = TileMap::SLOPES; // TileSlopes [N,E,w,S]
          m_autotileOrientation[currentLayer] = static_cast<TileOrientation>(m_elevationOrientation);
        }
      }
      const TileData *tileData = getTileData(currentLayer);

      switch (m_mapNodeData[currentLayer].tileMap)
      {
      case TileMap::DEFAULT:
        m_clippingWidth = tileData->tiles.clippingWidth;
        if (m_mapNodeData[currentLayer].tileIndex != 0)
        {
          clipRect.x = m_clippingWidth * m_mapNodeData[currentLayer].tileIndex;
        }
        else
        {
          if (tileData->tileType == +TileType::POWERLINE &&
              std::any_of(layersPowerlinesCanCross.begin(), layersPowerlinesCanCross.end(),
                          [this](const Layer &_layer) { return this->m_mapNodeData[_layer].tileData; }))
          { // if we place a power line cross low terrain (eg, roads, water, flora)
            switch (m_autotileOrientation[currentLayer])
            {
            case TileOrientation::TILE_N_AND_S:
              m_autotileOrientation[currentLayer] = TileOrientation::TILE_N_AND_S_CROSS;
              break;
            case TileOrientation::TILE_E_AND_W:
              m_autotileOrientation[currentLayer] = TileOrientation::TILE_E_AND_W_CROSS;
              break;
            default:
              break;
            }
          }
          // only check for rectangular roads when there are frames for it. Spritesheets with rect-roads have 20 items
          if (GameStates::instance().rectangularRoads && tileData->tiles.count == 20)
          {
            switch (m_autotileOrientation[currentLayer])
            {
            case TileOrientation::TILE_S_AND_W:
              m_autotileOrientation[currentLayer] = TileOrientation::TILE_S_AND_W_RECT;
              break;
            case TileOrientation::TILE_S_AND_E:
              m_autotileOrientation[currentLayer] = TileOrientation::TILE_S_AND_E_RECT;
              break;
            case TileOrientation::TILE_N_AND_E:
              m_autotileOrientation[currentLayer] = TileOrientation::TILE_N_AND_E_RECT;
              break;
            case TileOrientation::TILE_N_AND_W:
              m_autotileOrientation[currentLayer] = TileOrientation::TILE_N_AND_W_RECT;
              break;

            default:
              break;
            }
          }
          clipRect.x = m_clippingWidth * static_cast<int>(m_autotileOrientation[currentLayer]);
        }

        if (!m_mapNodeData[currentLayer].tileID.empty())
        {
          m_sprite->setClipRect(
              {clipRect.x + m_clippingWidth * tileData->tiles.offset, 0, m_clippingWidth, tileData->tiles.clippingHeight},
              static_cast<Layer>(currentLayer));
          m_sprite->setTexture(TileManager::instance().getTexture(m_mapNodeData[currentLayer].tileID),
                               static_cast<Layer>(currentLayer));
        }

        spriteCount = tileData->tiles.count;
        break;
      case TileMap::SHORE:
        m_clippingWidth = tileData->shoreTiles.clippingWidth;
        if (m_mapNodeData[currentLayer].tileIndex != 0)
        {
          clipRect.x = m_clippingWidth * m_mapNodeData[currentLayer].tileIndex;
        }
        else
        {
          clipRect.x = m_clippingWidth * static_cast<int>(m_autotileOrientation[currentLayer]);
        }

        if (!m_mapNodeData[currentLayer].tileID.empty())
        {
          m_sprite->setClipRect({clipRect.x + m_clippingWidth * tileData->shoreTiles.offset, 0, m_clippingWidth,
                                 tileData->shoreTiles.clippingHeight},
                                static_cast<Layer>(currentLayer));
          m_sprite->setTexture(TileManager::instance().getTexture(m_mapNodeData[currentLayer].tileID + "_shore"),
                               static_cast<Layer>(currentLayer));
        }

        spriteCount = tileData->shoreTiles.count;
        break;
      case TileMap::SLOPES:
        if (tileData->slopeTiles.fileName.empty())
        {
          break;
        }
        m_clippingWidth = tileData->slopeTiles.clippingWidth;
        clipRect.x = tileData->slopeTiles.clippingWidth * static_cast<int>(m_autotileOrientation[currentLayer]);
        spriteCount = tileData->slopeTiles.count;
        if (clipRect.x <= static_cast<int>(spriteCount) * m_clippingWidth)
        {
          m_sprite->setClipRect({clipRect.x + tileData->slopeTiles.offset * m_clippingWidth, 0, m_clippingWidth,
                                 tileData->slopeTiles.clippingHeight},
                                static_cast<Layer>(currentLayer));
          m_sprite->setTexture(TileManager::instance().getTexture(getTileID(currentLayer)), static_cast<Layer>(currentLayer));
        }
        break;
      default:
        break;
      }

      if (clipRect.x >= static_cast<int>(spriteCount) * m_clippingWidth)
      {
        m_mapNodeData[currentLayer].tileID = m_previousTileID;
        if (m_previousTileID.empty())
        {
          tileData = nullptr;
        }
        updateTexture(currentLayer);
      }
      m_sprite->spriteCount = spriteCount;
    }
  }
}

bool MapNode::isSlopeNode() const { return m_mapNodeData[Layer::TERRAIN].tileMap == TileMap::SLOPES; }

void MapNode::setCoordinates(const Point &newIsoCoordinates)
{
  m_isoCoordinates = newIsoCoordinates;
  m_sprite->isoCoordinates = m_isoCoordinates;
}

const MapNodeData &MapNode::getActiveMapNodeData() const { return m_mapNodeData[getTopMostActiveLayer()]; }

void MapNode::setMapNodeData(std::vector<MapNodeData> &&mapNodeData)
{
  m_mapNodeData.swap(mapNodeData);
  this->setNodeTransparency(Settings::instance().zoneLayerTransparency, Layer::ZONE);

  // updates the pointers to the tiles, after loading tileIDs from json
  for (auto &it : m_mapNodeData)
  {
    delete it.tileData;

    it.tileData = TileManager::instance().getTileData(it.tileID);
  }
}

void MapNode::demolishLayer(const Layer &layer)
{
  m_mapNodeData[layer].tileData = nullptr;
  m_mapNodeData[layer].tileID = "";
  m_autotileOrientation[layer] =
      TileOrientation::TILE_DEFAULT_ORIENTATION; // We need to reset TileOrientation, in case it's set (demolishing autotiles)
  m_mapNodeData[layer].origCornerPoint = this->getCoordinates();
  m_sprite->setRenderFlag(Layer::ZONE, true);
  m_sprite->clearSprite(layer);
}

void MapNode::demolishNode(const Layer &demolishLayer)
{
  // allow to delete a single layer only
  std::vector<Layer> layersToDemolish;
  if (demolishLayer == Layer::NONE)
  {
    layersToDemolish = {Layer::BUILDINGS,  Layer::UNDERGROUND, Layer::GROUND_DECORATION, Layer::ZONE, Layer::ROAD,
                        Layer::POWERLINES, Layer::FLORA};
  }
  else
  {
    layersToDemolish.push_back(demolishLayer);
  }

  for (auto &layer : layersToDemolish)
  {
    if (MapLayers::isLayerActive(layer) && isLayerOccupied(layer))
    {
      if ((GameStates::instance().demolishMode == DemolishMode::DEFAULT && getTileData(layer)->tileType == +TileType::ZONE) ||
          (GameStates::instance().demolishMode == DemolishMode::DE_ZONE && getTileData(layer)->tileType != +TileType::ZONE) ||
          (GameStates::instance().demolishMode == DemolishMode::GROUND_DECORATION &&
           getTileData(layer)->tileType != +TileType::GROUNDDECORATION))
      {
        continue;
      }
      this->demolishLayer(layer);
      if (layer == Layer::BUILDINGS)
      {
        for (auto coord : m_multiTileCoords)
        {
          MapFunctions::instance().getMapNode(coord).demolishNode(layer);
          MapFunctions::instance().getMapNode(coord).getSprite()->setRenderFlag(layer, true);
        }
        this->setNodeTransparency(0, Layer::BLUEPRINT);
      }
      updateTexture(demolishLayer);
    }
  }
}

const bool MapNode::isConductive() const
{
  std::vector<Layer> conductiveLayers = {Layer::BUILDINGS, Layer::POWERLINES, Layer::ZONE};

  for (auto layer : conductiveLayers)
  {
    if ((!isOriginNode() && MapFunctions::instance().getMapNode(m_originCoordinates).getTileData(layer)) ||
        (isOriginNode() && getTileData(layer)))
    { // if it's part of a multitilenode (not an originnode, has coordinates of the originnode and there's something placed on a conductive layer)
      // or if it an origin node and a conductive layer
      return true;
    }
  }
  return false;
}

const TileData *MapNode::getTileData(Layer layer) const
{
  if (!isOriginNode() )
  {
    return MapFunctions::instance().getMapNode(m_originCoordinates).getTileData(layer);
  }
  return m_mapNodeData[layer].tileData;
}

const std::string &MapNode::getTileID(Layer layer) const
{
  if (!isOriginNode())
  {
    return MapFunctions::instance().getMapNode(m_originCoordinates).getMapNodeDataForLayer(layer).tileID;
  }
  return m_mapNodeData[layer].tileID;
}

const MapNodeData &MapNode::getMapNodeDataForLayer(Layer layer) const
{

  if (!isOriginNode())
  {
    return MapFunctions::instance().getMapNode(m_originCoordinates).getMapNodeDataForLayer(layer);
  }
  return m_mapNodeData[layer];
};
