/**
 * @file   surfacesymbol.cpp
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

#include "surfacesymbol.h"

#include <iostream>
#include <typeinfo>
using std::cout;
using std::endl;

#include <QtWidgets>

#include "context.h"

SurfaceSymbol::SurfaceSymbol(const SurfaceRecord* rec):
  Symbol("Surface", "Surface", rec->polarity, rec->attrib),
  m_holeCount(0), m_islandCount(0)
{
  m_dcode = rec->dcode;
  m_polygons = rec->polygons;

  m_bounding = painterPath().boundingRect();
}

QString SurfaceSymbol::infoText(void)
{
  QPointF c = m_bounding.center();
  QString info = QString("Surface, XC=%1, YC=%2, Islands=%3, Holes=%4, %5") \
    .arg(c.x()).arg(c.y()) \
    .arg(m_islandCount).arg(m_holeCount) \
    .arg((m_polarity == P)? "POS": "NEG");
  return info;
}

QString SurfaceSymbol::longInfoText(void)
{
  QPointF c = m_bounding.center();
  qreal angle = getAngle();
  
  QString result(
      "Surface\n\n"
      "XC\t= %1\n"
      "YC\t= %2\n"
      "Islands\t= %3\n"
      "Holes\t= %4\n"
      "Polarity\t= %5\n"
  );
  
  result = result.arg(c.x()).arg(c.y())
    .arg(m_islandCount).arg(m_holeCount)
    .arg((m_polarity == P)? "POS": "NEG");
  
  //  CHANGED: Always show angle if valid (removed isTrace() condition)
  if (angle >= 0) {
    result += QString("Angle\t= %1\n").arg(angle, 0, 'f', 2);
  }
  
  return result;
}

QPainterPath SurfaceSymbol::painterPath(void)
{
  QPainterPath path;

  m_islandCount = 0;
  m_holeCount = 0;

  for (QList<PolygonRecord*>::iterator it = m_polygons.begin();
      it != m_polygons.end(); ++it) {
    PolygonRecord* rec = (*it);
    path.addPath(rec->painterPath());
    if (rec->poly_type == PolygonRecord::I) {
      ++m_islandCount;
    } else {
      ++m_holeCount;
    }
  }

  return path;
}

//  NEW: Get trace width from bounding box
qreal SurfaceSymbol::getWidth() const
{
  // Get bounding box dimensions
  qreal width = m_bounding.width();
  qreal height = m_bounding.height();
  
  // Validate dimensions
  if (width <= 0 || height <= 0) {
    return -1.0;  // Invalid dimensions
  }
  
  // For traces, width is the smaller dimension
  // (traces are elongated, so min dimension = trace width)
  return qMin(width, height);
}

//  NEW: Check if this surface is a trace (elongated shape)
bool SurfaceSymbol::isTrace() const
{
  qreal width = m_bounding.width();
  qreal height = m_bounding.height();
  
  if (width <= 0 || height <= 0) {
    return false;
  }
  
  qreal ratio = qMax(width, height) / qMin(width, height);
  
  //  CHANGED: Lower threshold from 3.0 to 2.0
  const qreal TRACE_ASPECT_RATIO_THRESHOLD = 2.0;  // Was 3.0
  
  // ADD DEBUG
  qDebug() << "isTrace() check:";
  qDebug() << "  width=" << width << ", height=" << height;
  qDebug() << "  ratio=" << ratio << ", threshold=" << TRACE_ASPECT_RATIO_THRESHOLD;
  qDebug() << "  isTrace=" << (ratio > TRACE_ASPECT_RATIO_THRESHOLD);
  
  return ratio > TRACE_ASPECT_RATIO_THRESHOLD;
}

//  NEW: Calculate dominant angle of surface trace
qreal SurfaceSymbol::getAngle() const
{
  // ADD DEBUG LOGGING
  qDebug() << "=== getAngle() called for Surface ===";
  qDebug() << "  m_polygons.isEmpty():" << m_polygons.isEmpty();
  
  // Method 1: Use first polygon segment
  if (m_polygons.isEmpty()) {
    qDebug() << "  -> No polygons, returning -1.0";
    return -1.0;
  }
  
  PolygonRecord* polygon = m_polygons.first();
  qDebug() << "  polygon->operations.isEmpty():" << polygon->operations.isEmpty();
  
  if (polygon->operations.isEmpty()) {
    // Fallback: Use bounding box diagonal
    qreal dx = m_bounding.width();
    qreal dy = m_bounding.height();
    
    qDebug() << "  -> No operations, using bounding box";
    qDebug() << "     dx =" << dx << ", dy =" << dy;
    
    if (dx == 0 && dy == 0) {
      qDebug() << "  -> Zero dimensions, returning -1.0";
      return -1.0;
    }
    
    qreal angle = qAtan2(dy, dx) * 180.0 / M_PI;
    if (angle < 0) angle += 360.0;
    
    qDebug() << "  -> Bounding box angle =" << angle << "degrees";
    return angle;
  }
  
  SurfaceOperation* op = polygon->operations.first();
  qreal sx = polygon->xbs;
  qreal sy = polygon->ybs;
  qreal ex, ey;
  
  qDebug() << "  Start point: (" << sx << ", " << sy << ")";
  
  if (op->type == SurfaceOperation::SEGMENT) {
    ex = op->x;
    ey = op->y;
    qDebug() << "  Operation type: SEGMENT";
  } else {  // CURVE
    ex = op->xe;
    ey = op->ye;
    qDebug() << "  Operation type: CURVE";
  }
  
  qDebug() << "  End point: (" << ex << ", " << ey << ")";
  
  qreal dx = ex - sx;
  qreal dy = ey - sy;
  
  qDebug() << "  dx =" << dx << ", dy =" << dy;
  
  if (dx == 0 && dy == 0) {
    qDebug() << "  -> Zero displacement, returning -1.0";
    return -1.0;
  }
  
  // Calculate angle (counter-clockwise from X-axis)
  qreal angle = qAtan2(dy, dx) * 180.0 / M_PI;
  
  qDebug() << "  Raw angle (before normalization):" << angle;
  
  // Normalize to 0-360 range
  if (angle < 0) angle += 360.0;
  
  qDebug() << "  Final angle:" << angle << "degrees";
  return angle;
}

//  NEW: Measure trace thickness at pixel coordinate
qreal SurfaceSymbol::measureTraceThickness(const QImage& image, int pixelX, int pixelY,
                                            qreal angle, const QRectF& sceneRect, 
                                            const QRectF& targetRect, const QColor& highlightColor)
{
  qDebug() << "=== measureTraceThickness() called ===";
  qDebug() << "  Pixel: (" << pixelX << ", " << pixelY << ")";
  qDebug() << "  Angle:" << angle << "degrees";
  qDebug() << "  Image size:" << image.width() << "x" << image.height();
  
  // Step 1: Validate input
  if (pixelX < 0 || pixelX >= image.width() || pixelY < 0 || pixelY >= image.height()) {
    qDebug() << "  -> Pixel out of bounds";
    return -1.0;
  }
  
  // Step 2: Check if pixel is on trace (has highlight color)
  QRgb centerPixel = image.pixel(pixelX, pixelY);
  QColor centerColor(centerPixel);
  
  // Color matching with tolerance
  auto colorMatch = [](const QColor &c1, const QColor &c2, int tolerance = 40) -> bool {
    return qAbs(c1.red() - c2.red()) <= tolerance &&
           qAbs(c1.green() - c2.green()) <= tolerance &&
           qAbs(c1.blue() - c2.blue()) <= tolerance;
  };
  
  if (!colorMatch(centerColor, highlightColor)) {
    qDebug() << "  -> Pixel is not on trace (color mismatch)";
    qDebug() << "     Center RGB(" << centerColor.red() << "," << centerColor.green() << "," << centerColor.blue() << ")";
    qDebug() << "     Expected RGB(" << highlightColor.red() << "," << highlightColor.green() << "," << highlightColor.blue() << ")";
    return -1.0;
  }
  
  qDebug() << "  -> Pixel IS on trace";
  
  // Step 3: Normalize angle to [0, 180)
  qreal normalizedAngle = angle;
  if (normalizedAngle >= 180.0) {
    normalizedAngle = normalizedAngle - 180.0;
  }
  
  qDebug() << "  Normalized angle:" << normalizedAngle;
  
  // Step 4: Choose scan direction based on angle
  bool scanHorizontal = false;
  
  //  YOUR RULE: angle = 90°  scan horizontal (w = l)
  // All other angles  scan vertical (w = l * cos(angle))
  if (qAbs(normalizedAngle - 90.0) < 5.0) {  // 85° - 95° range
    scanHorizontal = true;
    qDebug() << "  -> Scanning HORIZONTAL (angle near 90°)";
  } else {
    scanHorizontal = false;
    qDebug() << "  -> Scanning VERTICAL";
  }
  
  // Step 5: Scan to find edges
  int l1 = 0;  // Distance to edge in negative direction
  int l2 = 0;  // Distance to edge in positive direction
  
  if (scanHorizontal) {
    // Scan LEFT (negative X)
    for (int x = pixelX - 1; x >= 0; x--) {
      QRgb pixel = image.pixel(x, pixelY);
      if (!colorMatch(QColor(pixel), highlightColor)) {
        break;  // Hit edge
      }
      l1++;
    }
    
    // Scan RIGHT (positive X)
    for (int x = pixelX + 1; x < image.width(); x++) {
      QRgb pixel = image.pixel(x, pixelY);
      if (!colorMatch(QColor(pixel), highlightColor)) {
        break;  // Hit edge
      }
      l2++;
    }
    
    qDebug() << "  Horizontal scan: left=" << l1 << "px, right=" << l2 << "px";
    
  } else {
    // Scan UP (negative Y)
    for (int y = pixelY - 1; y >= 0; y--) {
      QRgb pixel = image.pixel(pixelX, y);
      if (!colorMatch(QColor(pixel), highlightColor)) {
        break;  // Hit edge
      }
      l1++;
    }
    
    // Scan DOWN (positive Y)
    for (int y = pixelY + 1; y < image.height(); y++) {
      QRgb pixel = image.pixel(pixelX, y);
      if (!colorMatch(QColor(pixel), highlightColor)) {
        break;  // Hit edge
      }
      l2++;
    }
    
    qDebug() << "  Vertical scan: up=" << l1 << "px, down=" << l2 << "px";
  }
  
  // Step 6: Calculate total pixel length
  int totalPixels = l1 + l2 + 1;  // +1 for center pixel
  qDebug() << "  Total pixels:" << totalPixels;
  
  if (totalPixels <= 1) {
    qDebug() << "  -> Trace too small to measure";
    return -1.0;
  }
  
  // Step 7: Convert pixels to scene units (inches)
  qreal scaleX = sceneRect.width() / targetRect.width();
  qreal scaleY = sceneRect.height() / targetRect.height();
  
  qreal pixelSize;
  if (scanHorizontal) {
    pixelSize = scaleX;  // inches per pixel in X direction
  } else {
    pixelSize = scaleY;  // inches per pixel in Y direction
  }
  
  qreal l_inches = totalPixels * pixelSize;
  qDebug() << "  Length in scene units:" << l_inches << "inches";
  
  // Step 8: Calculate actual thickness
  qreal w_inches;
  
  if (scanHorizontal) {
    //  YOUR RULE: Horizontal scan (angle = 90°)  w = l
    w_inches = l_inches;
    qDebug() << "  -> w = l (horizontal scan)";
  } else {
    //  YOUR RULE: Vertical scan  w = l * cos(angle)
    qreal angleRad = normalizedAngle * M_PI / 180.0;
    w_inches = l_inches * qCos(angleRad);
    qDebug() << "  -> w = l * cos(" << normalizedAngle << "°) = " << l_inches << " * " << qCos(angleRad);
  }
  
  // Step 9: Convert to millimeters
  qreal w_mm = w_inches * 25.4;
  
  qDebug() << "  Final thickness:" << w_mm << "mm (" << w_inches << " inches)";
  
  return w_mm;
}
