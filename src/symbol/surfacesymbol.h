/**
 * @file   surfacesymbol.h
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

#ifndef __SURFACESYMBOL_H__
#define __SURFACESYMBOL_H__

#include "symbol.h"

#include <QtWidgets>

#include "featuresparser.h"
#include "record.h"

class SurfaceSymbol: public Symbol {
public:
  SurfaceSymbol(const SurfaceRecord* rec);

  virtual QString infoText(void);
  virtual QString longInfoText(void);
  virtual QPainterPath painterPath(void);

  //  NEW: Override getWidth() for trace filtering
  virtual qreal getWidth() const override;
  
  //  NEW: Override isTrace() to detect trace-like shapes
  virtual bool isTrace() const override;

  //  NEW: Override getAngle() for trace angle calculation
  virtual qreal getAngle() const override;

  //  NEW: Measure actual trace thickness at a specific pixel coordinate
  // Returns thickness in mm, or -1.0 if measurement fails
  static qreal measureTraceThickness(const QImage& image, int pixelX, int pixelY,
                                      qreal angle, const QRectF& sceneRect, 
                                      const QRectF& targetRect, const QColor& highlightColor);

private:
  int m_dcode;
  int m_holeCount;
  int m_islandCount;
  QList<PolygonRecord*> m_polygons;
};

#endif /* __SURFACESYMBOL_H__ */
