/**
 * @file   graphicslayerscene.cpp
 * @author Wei-Ning Huang (AZ) <aitjcize@gmail.com>
 *
 * Copyright (C) 2012 - 2014 Wei-Ning Huang (AZ) <aitjcize@gmail.com>
 * All Rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "graphicslayerscene.h"
#include "graphicslayer.h"
#include "layer.h"  // ADD THIS: Need Layer class for layer()
#include "context.h"

#include <QtWidgets>
#include <QSet>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>

GraphicsLayerScene::GraphicsLayerScene(QObject* parent):
  QGraphicsScene(parent), 
  m_graphicsLayer(nullptr),  
  m_highlight(false)
{
  setItemIndexMethod(NoIndex);
}

GraphicsLayerScene::~GraphicsLayerScene()
{
}

void GraphicsLayerScene::setGraphicsLayer(GraphicsLayer* layer)
{
  m_graphicsLayer = layer;
}

bool GraphicsLayerScene::highlight(void)
{
  return m_highlight;
}

void GraphicsLayerScene::setHighlightEnabled(bool status)
{
  m_highlight = status;
}

void GraphicsLayerScene::clearHighlight(void)
{
  for (int i = 0; i < m_selectedSymbols.size(); ++i) {
    m_selectedSymbols[i]->restoreColor();
  }
  m_selectedSymbols.clear();
  
  if (m_graphicsLayer) {
    m_graphicsLayer->forceUpdate();
  }
}

void GraphicsLayerScene::updateSelection(Symbol* symbol)
{
  clearHighlight();
  m_selectedSymbols.append(symbol);
  emit featureSelected(symbol);
}

void GraphicsLayerScene::toggleSelection(Symbol* symbol)
{
  if (m_selectedSymbols.contains(symbol)) {
    symbol->restoreColor();
    m_selectedSymbols.removeOne(symbol);
  } else {
    m_selectedSymbols.append(symbol);
  }
  
  emit featureSelected(symbol);
  
  if (m_graphicsLayer) {
    m_graphicsLayer->forceUpdate();
  }
}

// UPDATED: Toggle connected symbol groups (keep other groups highlighted)
void GraphicsLayerScene::selectConnectedSymbols(Symbol* startSymbol)
{
  if (!startSymbol) {
    return;
  }

  // Use QSet to track visited symbols (prevents infinite loops)
  QSet<Symbol*> connectedSymbols;
  
  // Find all connected symbols
  findConnectedSymbols(startSymbol, connectedSymbols);

  // Check if this group is already fully selected
  bool allSelected = true;
  for (Symbol* sym : connectedSymbols) {
    if (!m_selectedSymbols.contains(sym)) {
      allSelected = false;
      break;
    }
  }

  if (allSelected) {
    // If all connected symbols are selected DESELECT the entire group
    for (Symbol* sym : connectedSymbols) {
      sym->setSelected(false);
      sym->restoreColor();
      m_selectedSymbols.removeOne(sym);
    }
  } else {
    // If not all selected SELECT the entire group (add to existing selection)
    // UPDATED: Use dynamic highlight color from context
    QColor highlightColor = ctx.highlight_color;
    
    for (Symbol* sym : connectedSymbols) {
      if (!m_selectedSymbols.contains(sym)) {
        sym->setSelected(true);
        
        // Save current color
        sym->savePrevColor();
        
        // Set highlight color dynamically
        sym->setPen(QPen(highlightColor, 0));
        sym->setBrush(highlightColor);
        sym->update();
        
        m_selectedSymbols.append(sym);
      }
    }
  }

  // Emit signal with the start symbol
  emit featureSelected(startSymbol);
  
  // Force redraw
  if (m_graphicsLayer) {
    m_graphicsLayer->forceUpdate();
  }
}

// UNCHANGED: Recursive flood-fill to find all connected symbols
void GraphicsLayerScene::findConnectedSymbols(Symbol* symbol, QSet<Symbol*>& visited, qreal tolerance)
{
  if (!symbol || visited.contains(symbol)) {
    return;
  }

  visited.insert(symbol);

  QList<QGraphicsItem*> allItems = items();

  for (QGraphicsItem* item : allItems) {
    Symbol* otherSymbol = dynamic_cast<Symbol*>(item);
    
    if (!otherSymbol || visited.contains(otherSymbol)) {
      continue;
    }

    if (areSymbolsConnected(symbol, otherSymbol, tolerance)) {
      findConnectedSymbols(otherSymbol, visited, tolerance);
    }
  }
}

// UNCHANGED: Check if two symbols are touching/connected
bool GraphicsLayerScene::areSymbolsConnected(Symbol* sym1, Symbol* sym2, qreal tolerance)
{
  if (!sym1 || !sym2) {
    return false;
  }

  QRectF rect1 = sym1->sceneBoundingRect();
  QRectF rect2 = sym2->sceneBoundingRect();

  rect1.adjust(-tolerance, -tolerance, tolerance, tolerance);
  rect2.adjust(-tolerance, -tolerance, tolerance, tolerance);

  if (!rect1.intersects(rect2)) {
    return false;
  }

  QPainterPath shape1 = sym1->mapToScene(sym1->shape());
  QPainterPath shape2 = sym2->mapToScene(sym2->shape());

  return shape1.intersects(shape2);
}

// UPDATED: Select all traces with width <= maxWidth
void GraphicsLayerScene::selectTracesByWidth(qreal maxWidth)
{
  QSet<Symbol*> alreadyProcessed;
  for (Symbol* sym : m_selectedSymbols) {
    alreadyProcessed.insert(sym);
  }
  
  QList<QGraphicsItem*> allItems = items();
  
  int selectedCount = 0;
  int totalSurfaceCount = 0;
  int traceCount = 0;
  int groupCount = 0;
  
  qDebug() << "=== Select Traces by Width (with Connected Symbols) ===";
  qDebug() << "Max width threshold:" << maxWidth << "inches (" << (maxWidth * 25.4) << "mm)";
  
  QList<Symbol*> matchingTraces;
  
  for (QGraphicsItem* item : allItems) {
    Symbol* symbol = dynamic_cast<Symbol*>(item);
    
    if (!symbol) {
      continue;
    }
    
    if (symbol->name() != "Surface") {
      continue;
    }
    
    totalSurfaceCount++;
    
    if (!symbol->isTrace()) {
      continue;
    }
    
    traceCount++;
    
    qreal width = symbol->getWidth();
    
    if (width < 0) {
      qDebug() << "Warning: Symbol has invalid width:" << width;
      continue;
    }
    
    if (width <= maxWidth) {
      matchingTraces.append(symbol);
      selectedCount++;
      
      if (selectedCount <= 5) {
        qDebug() << "  Found trace:" << selectedCount 
                 << "width =" << width << "inches (" << (width * 25.4) << "mm)";
      }
    }
  }
  
  qDebug() << "Found" << matchingTraces.size() << "traces matching criteria";
  
  // UPDATED: Use dynamic highlight color from context
  QColor highlightColor = ctx.highlight_color;
  
  for (Symbol* trace : matchingTraces) {
    if (alreadyProcessed.contains(trace)) {
      continue;
    }
    
    QSet<Symbol*> connectedSymbols;
    findConnectedSymbols(trace, connectedSymbols);
    
    groupCount++;
    
    if (groupCount <= 3) {
      qDebug() << "  Group" << groupCount << ":" << connectedSymbols.size() << "connected symbols";
    }
    
    // Highlight entire connected group with dynamic color
    for (Symbol* sym : connectedSymbols) {
      if (!m_selectedSymbols.contains(sym)) {
        sym->setSelected(true);
        sym->savePrevColor();
        
        // Use dynamic highlight color
        sym->setPen(QPen(highlightColor, 0));
        sym->setBrush(highlightColor);
        sym->update();
        
        m_selectedSymbols.append(sym);
        alreadyProcessed.insert(sym);
      }
    }
  }
  
  if (m_graphicsLayer) {
    m_graphicsLayer->forceUpdate();
  }
  
  qDebug() << "=== Summary ===";
  qDebug() << "Total surface symbols:" << totalSurfaceCount;
  qDebug() << "Trace-like surfaces:" << traceCount;
  qDebug() << "Matching traces (width <=" << maxWidth << "):" << selectedCount;
  qDebug() << "Connected groups:" << groupCount;
  qDebug() << "Total selected symbols:" << m_selectedSymbols.size();
  qDebug() << "";
}

// FIXED: Get layer name helper method
QString GraphicsLayerScene::getLayerName() const
{
  if (!m_graphicsLayer) {
    return QString();
  }
  
  // Try to cast to Layer to access layer() method
  Layer* layer = dynamic_cast<Layer*>(m_graphicsLayer);
  if (layer) {
    return layer->layer();
  }
  
  return QString(); // Return empty if not a Layer
}

// FIXED: Export highlight data to JSON
QJsonObject GraphicsLayerScene::exportHighlightData() const
{
  QJsonObject root;
  
  // Metadata
  root["version"] = "1.0";
  root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  root["highlightCount"] = m_selectedSymbols.size();
  
  // Layer info - use helper method
  QString layerName = getLayerName();
  if (!layerName.isEmpty()) {
    root["layerName"] = layerName;
  }
  
  // Highlighted symbols
  QJsonArray symbolsArray;
  for (Symbol* symbol : m_selectedSymbols) {
    QJsonObject symbolObj;
    
    // Store symbol identifier
    symbolObj["id"] = getSymbolIdentifier(symbol);
    
    // Store symbol properties for verification
    symbolObj["name"] = symbol->name();
    symbolObj["type"] = QString(typeid(*symbol).name());
    
    // Store bounding box for position verification
    QRectF bounds = symbol->boundingRect();
    QJsonObject boundsObj;
    boundsObj["x"] = bounds.x();
    boundsObj["y"] = bounds.y();
    boundsObj["width"] = bounds.width();
    boundsObj["height"] = bounds.height();
    symbolObj["bounds"] = boundsObj;
    
    // FIXED: Store color from current context (not from symbol's pen)
    // Since symbols may have been changed dynamically
    QJsonObject colorObj;
    colorObj["r"] = ctx.highlight_color.red();
    colorObj["g"] = ctx.highlight_color.green();
    colorObj["b"] = ctx.highlight_color.blue();
    symbolObj["highlightColor"] = colorObj;
    
    symbolsArray.append(symbolObj);
  }
  
  root["symbols"] = symbolsArray;
  
  return root;
}

// FIXED: Import highlight data from JSON
bool GraphicsLayerScene::importHighlightData(const QJsonObject& data)
{
  // Clear existing highlights
  clearHighlight();
  
  // Verify version
  QString version = data["version"].toString();
  if (version != "1.0") {
    qWarning() << "Unsupported highlight data version:" << version;
    return false;
  }
  
  // Verify layer name (optional warning)
  QString savedLayer = data["layerName"].toString();
  QString currentLayer = getLayerName();
  
  if (!savedLayer.isEmpty() && !currentLayer.isEmpty() && savedLayer != currentLayer) {
    qWarning() << "Layer name mismatch: saved=" << savedLayer 
               << "current=" << currentLayer;
    // Continue anyway, user might want to apply to different layer
  }
  
  // Load symbols
  QJsonArray symbolsArray = data["symbols"].toArray();
  int loadedCount = 0;
  int failedCount = 0;
  
  //  Use dynamic highlight color from context
  QColor highlightColor = ctx.highlight_color;
  
  for (const QJsonValue& val : symbolsArray) {
    QJsonObject symbolObj = val.toObject();
    QString id = symbolObj["id"].toString();
    
    // Try to find symbol by identifier
    Symbol* symbol = findSymbolByIdentifier(id);
    
    if (symbol) {
      // Verify bounds match (with small tolerance for floating point)
      QJsonObject boundsObj = symbolObj["bounds"].toObject();
      QRectF savedBounds(
        boundsObj["x"].toDouble(),
        boundsObj["y"].toDouble(),
        boundsObj["width"].toDouble(),
        boundsObj["height"].toDouble()
      );
      
      QRectF currentBounds = symbol->boundingRect();
      qreal tolerance = 0.001;
      
      bool boundsMatch = 
        qAbs(savedBounds.x() - currentBounds.x()) < tolerance &&
        qAbs(savedBounds.y() - currentBounds.y()) < tolerance &&
        qAbs(savedBounds.width() - currentBounds.width()) < tolerance &&
        qAbs(savedBounds.height() - currentBounds.height()) < tolerance;
      
      if (boundsMatch) {
        // Apply highlight with current context color
        symbol->setSelected(true);
        symbol->savePrevColor();
        symbol->setPen(QPen(highlightColor, 0));
        symbol->setBrush(highlightColor);
        symbol->update();
        
        m_selectedSymbols.append(symbol);
        loadedCount++;
      } else {
        qWarning() << "Symbol bounds mismatch for ID:" << id;
        failedCount++;
      }
    } else {
      qWarning() << "Symbol not found for ID:" << id;
      failedCount++;
    }
  }
  
  qDebug() << "Highlight import: loaded=" << loadedCount 
           << "failed=" << failedCount;
  
  // Force redraw
  if (m_graphicsLayer) {
    m_graphicsLayer->forceUpdate();
  }
  
  return loadedCount > 0;
}

// NEW: Generate unique identifier for a symbol
QString GraphicsLayerScene::getSymbolIdentifier(Symbol* symbol) const
{
  if (!symbol) return QString();
  
  // Create identifier from symbol properties
  QStringList parts;
  parts << symbol->name();
  parts << QString(typeid(*symbol).name());
  
  QRectF bounds = symbol->boundingRect();
  parts << QString::number(bounds.x(), 'f', 6);
  parts << QString::number(bounds.y(), 'f', 6);
  parts << QString::number(bounds.width(), 'f', 6);
  parts << QString::number(bounds.height(), 'f', 6);
  
  QString combined = parts.join("|");
  
  // Generate MD5 hash for compact identifier
  QByteArray hash = QCryptographicHash::hash(
    combined.toUtf8(), 
    QCryptographicHash::Md5
  );
  
  return QString(hash.toHex());
}

// NEW: Find symbol by identifier
Symbol* GraphicsLayerScene::findSymbolByIdentifier(const QString& identifier) const
{
  if (!m_graphicsLayer) return nullptr;
  
  // Search through all items in the scene
  QList<QGraphicsItem*> allItems = items();
  
  for (QGraphicsItem* item : allItems) {
    Symbol* symbol = dynamic_cast<Symbol*>(item);
    if (symbol) {
      QString symbolId = getSymbolIdentifier(symbol);
      if (symbolId == identifier) {
        return symbol;
      }
    }
  }
  
  return nullptr;
}
