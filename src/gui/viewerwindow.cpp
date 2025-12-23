/**
 * @file   viewerwindow.cpp
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

#include "viewerwindow.h"
#include "ui_viewerwindow.h"

#include <cmath>
#include <QtWidgets>
#include <QDebug>
#include <QDir>
#include <QBuffer>
#include <QPixmap>
#include <QTimer>
#include <QDateTime>
#include <QEventLoop>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>

#include "context.h"
#include "gotocoordinatedialog.h"
#include "layerinfobox.h"
#include "logger.h"
#include "settingsdialog.h"
#include "settings.h"
#include "restapi/restapiserver.h"
#include "graphicslayerscene.h"

ViewerWindow::ViewerWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::ViewerWindow), m_displayUnit(U_INCH),
  m_activeInfoBox(NULL), m_transition(false), m_restApiServer(nullptr),
  m_highlightColor(QColor(0, 0, 255))
{
  ui->setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);

  ctx.highlight_color = QColor(0, 0, 255);

  loadColorConfig();

  m_cursorCoordLabel = new QLabel();
  m_featureDetailLabel = new QLabel();
  m_featureDetailLabel->setAlignment(Qt::AlignVCenter);
  m_cursorCoordLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
  statusBar()->addPermanentWidget(m_featureDetailLabel);
  statusBar()->addPermanentWidget(m_cursorCoordLabel, 1);

  QComboBox* unitCombo = new QComboBox;
  unitCombo->addItem("Inch");
  unitCombo->addItem("MM");
  statusBar()->addPermanentWidget(unitCombo);

  m_featurePropertiesDialog = new FeaturePropertiesDialog(this);
  m_goToCoordinateDialog = new GoToCoordinateDialog(this);

  connect(unitCombo, SIGNAL(currentIndexChanged(int)), this,
      SLOT(unitChanged(int)));

  connect(ui->viewWidget->scene(), SIGNAL(mouseMove(QPointF)), this,
      SLOT(updateCursorCoord(QPointF)));
  connect(ui->viewWidget->scene(), SIGNAL(measureRectSelected(QRectF)), this,
      SLOT(updateMeasureResult(QRectF)));
  connect(ui->viewWidget->scene(), SIGNAL(featureSelected(Symbol*)), this,
      SLOT(updateFeatureDetail(Symbol*)));
  connect(ui->viewWidget->scene(), SIGNAL(featureSelected(Symbol*)),
      m_featurePropertiesDialog, SLOT(update(Symbol*)));

  connect(ui->miniMapView, SIGNAL(minimapRectSelected(QRectF)), ui->viewWidget,
      SLOT(zoomToRect(QRectF)));
  connect(ui->viewWidget, SIGNAL(sceneRectChanged(QRectF)), ui->miniMapView,
      SLOT(redrawSceneRect(QRectF)));

  connect(this, SIGNAL(bgColorChanged(QColor)), ui->viewWidget,
      SLOT(setBackgroundColor(QColor)));
  connect(this, SIGNAL(bgColorChanged(QColor)), ui->miniMapView,
      SLOT(setBackgroundColor(QColor)));

  ui->viewWidget->setFocus(Qt::MouseFocusReason);
  ui->actionAreaZoom->setChecked(true);
  startRestApiServer(8686);

  QToolBar* traceToolBar = addToolBar("Trace Selection");
  
  QPushButton* btnR1 = new QPushButton("R1", this);
  QPushButton* btnR2 = new QPushButton("R2", this);
  QPushButton* btnR3 = new QPushButton("R3", this);
  
  btnR1->setToolTip("Select traces < 6 mils (0.15mm)");
  btnR2->setToolTip("Select traces < 10 mils (0.25mm)");
  btnR3->setToolTip("Select traces < 15 mils (0.38mm)");
  
  btnR1->setFixedSize(40, 30);
  btnR2->setFixedSize(40, 30);
  btnR3->setFixedSize(40, 30);
  
  connect(btnR1, &QPushButton::clicked, this, &ViewerWindow::on_actionSelectTraceR1_triggered);
  connect(btnR2, &QPushButton::clicked, this, &ViewerWindow::on_actionSelectTraceR2_triggered);
  connect(btnR3, &QPushButton::clicked, this, &ViewerWindow::on_actionSelectTraceR3_triggered);
  
  traceToolBar->addWidget(new QLabel("Trace Filter: "));
  traceToolBar->addWidget(btnR1);
  traceToolBar->addWidget(btnR2);
  traceToolBar->addWidget(btnR3);
  
  traceToolBar->addSeparator();
  QPushButton* btnHighlightColor = new QPushButton("🎨", this);
  btnHighlightColor->setToolTip("Toggle highlight color (Blue/Purple)");
  btnHighlightColor->setFixedSize(40, 30);
  btnHighlightColor->setStyleSheet("QPushButton { background-color: rgb(0, 0, 255); color: white; }");
  connect(btnHighlightColor, &QPushButton::clicked, this, &ViewerWindow::on_actionToggleHighlightColor_triggered);
  traceToolBar->addWidget(new QLabel(" Color: "));
  traceToolBar->addWidget(btnHighlightColor);
  
  m_highlightColorButton = btnHighlightColor;

  QMenu* highlightMenu = menuBar()->addMenu(tr("&Highlight"));
  
  QAction* actionSaveHighlight = new QAction(tr("&Save Highlights..."), this);
  actionSaveHighlight->setShortcut(QKeySequence::Save);
  actionSaveHighlight->setStatusTip(tr("Save highlighted symbols to JSON file"));
  connect(actionSaveHighlight, &QAction::triggered, 
          this, &ViewerWindow::on_actionSaveHighlight_triggered);
  
  QAction* actionLoadHighlight = new QAction(tr("&Load Highlights..."), this);
  actionLoadHighlight->setShortcut(QKeySequence::Open);
  actionLoadHighlight->setStatusTip(tr("Load highlighted symbols from JSON file"));
  connect(actionLoadHighlight, &QAction::triggered, 
          this, &ViewerWindow::on_actionLoadHighlight_triggered);
  
  highlightMenu->addAction(actionSaveHighlight);
  highlightMenu->addAction(actionLoadHighlight);
  highlightMenu->addSeparator();
  
  QAction* actionClearHighlight = new QAction(tr("&Clear Highlights"), this);
  actionClearHighlight->setShortcut(QKeySequence(Qt::Key_Escape));
  actionClearHighlight->setStatusTip(tr("Clear all highlighted symbols"));
  connect(actionClearHighlight, &QAction::triggered, 
          this, &ViewerWindow::on_actionClearHighlight_triggered);
  
  highlightMenu->addAction(actionClearHighlight);
}

ViewerWindow::~ViewerWindow()
{
  delete ui;
  delete m_featurePropertiesDialog;
  delete m_goToCoordinateDialog;
}

void ViewerWindow::setJob(QString job)
{
  m_job = job;
}

void ViewerWindow::setStep(QString step)
{
  m_step = step;
  setWindowTitle(QString("CAMViewer::%1::%2").arg(m_job).arg(m_step));
}

void ViewerWindow::setLayers(const QStringList& layers,
    const QStringList& types)
{
  ui->viewWidget->clearScene();
  ui->viewWidget->loadProfile(m_step);
  ui->miniMapView->loadProfile(m_step);

  QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(ui->scrollWidget->layout());
  clearLayout(layout, true);
  QString pathTmpl = "steps/%1/layers/%2";

  for (int i = 0; i < layers.count(); ++i) {
    LayerInfoBox *l = new LayerInfoBox(layers[i], m_step, types[i]);

    connect(l, SIGNAL(toggled(bool)), this, SLOT(toggleShowLayer(bool)));
    connect(l, SIGNAL(activated(bool)), this, SLOT(layerActivated(bool)));

    m_SelectorMap[layers[i]] = l;
    layout->addWidget(l);
  }
  layout->addStretch();
}

void ViewerWindow::clearLayout(QLayout* layout, bool deleteWidgets)
{
  while (QLayoutItem* item = layout->takeAt(0))
  {
    if (deleteWidgets)
    {
      if (QWidget* widget = item->widget())
        delete widget;
    }
    else if (QLayout* childLayout = item->layout())
      clearLayout(childLayout, deleteWidgets);
    delete item;
  }
}

void ViewerWindow::showLayer(QString name)
{
  LayerInfoBox* infobox = m_SelectorMap[name];
  infobox->toggle();
}

void ViewerWindow::show(void)
{
  QMainWindow::show();
  ui->viewWidget->initialZoom();
  ui->miniMapView->zoomToAll();
}

void ViewerWindow::toggleShowLayer(bool selected)
{
  LayerInfoBox* infobox = dynamic_cast<LayerInfoBox*>(sender());
  if (!selected) {
    ui->viewWidget->addLayer(infobox->layer());
    infobox->setColor(nextColor());
    infobox->layer()->setShowOutline(ui->actionShowOutline->isChecked());
    infobox->layer()->setShowStepRepeat(ui->actionShowStepRepeat->isChecked());

    m_visibles.append(infobox);
    if (m_visibles.size() == 1) {
      infobox->setActive(true);
    }
  } else {
    int index = m_colors.indexOf(infobox->color());
    m_colorsMap[index] = false;
    ui->viewWidget->removeLayer(infobox->layer());
    m_visibles.removeOne(infobox);

    if (infobox->isActive()) {
      if (m_visibles.size()) {
        m_visibles.last()->setActive(true);
      }
    }
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::layerActivated(bool status)
{
  LayerInfoBox* infobox = dynamic_cast<LayerInfoBox*>(sender());
  if (status) {
    if (m_activeInfoBox && m_activeInfoBox != infobox) {
      m_activeInfoBox->setActive(false);
    }
    m_activeInfoBox = infobox;
    if (ui->actionHighlight->isChecked()) {
      m_activeInfoBox->layer()->setHighlightEnabled(true);
    }
  } else {
    infobox->layer()->setHighlightEnabled(false);
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

QColor ViewerWindow::nextColor(void)
{
  for (int i = 0; i < m_colors.size(); ++i) {
    if (!m_colorsMap[i]) {
      m_colorsMap[i] = true;
      return m_colors[i];
    }
  }
  return Qt::red;
}

void ViewerWindow::loadColorConfig()
{
  ctx.bg_color = QColor(SETTINGS->get("color", "BG").toString());

  m_colors.clear();
  for(int i = 0; i < 6; ++i) {
    m_colors << QColor(SETTINGS->get("Color",
          QString("C%1").arg(i + 1)).toString());
  }

  for (int i = 0; i < m_colors.size(); ++i) {
    m_colorsMap[i] = false;
  }

  for (int i = 0; i < m_visibles.size(); ++i) {
    m_visibles[i]->setColor(nextColor());
    m_visibles[i]->layer()->forceUpdate();
  }

  emit bgColorChanged(ctx.bg_color);
}

void ViewerWindow::unitChanged(int index)
{
  m_displayUnit = (DisplayUnit)index;
}

void ViewerWindow::updateCursorCoord(QPointF pos)
{
  QString text;
  if (m_displayUnit == U_INCH) {
    text = QString::asprintf("(%f, %f)", pos.x(), -pos.y());
  } else {
    text = QString::asprintf("(%f, %f)", pos.x() * 25.4, -pos.y() * 25.4);
  }
  m_cursorCoordLabel->setText(text);
}

void ViewerWindow::updateFeatureDetail(Symbol* symbol)
{
  m_featureDetailLabel->setText(symbol->infoText());
}

void ViewerWindow::updateMeasureResult(QRectF rect)
{
  QString result("DX=%1, DY=%2, D=%3");
  m_featureDetailLabel->setText(result.arg(rect.width()).arg(rect.height())
    .arg(qSqrt(rect.width() * rect.width() + rect.height() * rect.height())));
}

void ViewerWindow::on_actionSetColor_triggered(void)
{
  SettingsDialog dialog;
  dialog.exec();
  loadColorConfig();
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionZoomIn_triggered(void)
{
  ui->viewWidget->scaleView(2);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionZoomOut_triggered(void)
{
  ui->viewWidget->scaleView(0.5);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionHome_triggered(void)
{
  ui->viewWidget->zoomToAll();
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionMousePan_toggled(bool checked)
{
  Q_UNUSED(checked);
  if (m_transition) {
    return;
  }
  m_transition = true;
  ui->actionAreaZoom->setChecked(false);
  ui->actionHighlight->setChecked(false);
  ui->actionMeasure->setChecked(false);
  m_transition = false;
  ui->viewWidget->setZoomMode(ODBPPGraphicsView::MousePan);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionAreaZoom_toggled(bool checked)
{
  Q_UNUSED(checked);
  if (m_transition) {
    return;
  }
  m_transition = true;
  ui->actionMousePan->setChecked(false);
  ui->actionHighlight->setChecked(false);
  ui->actionMeasure->setChecked(false);
  m_transition = false;
  ui->viewWidget->setZoomMode(ODBPPGraphicsView::AreaZoom);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionPanLeft_triggered(void)
{
  ui->viewWidget->scrollView(-500, 0);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionPanRight_triggered(void)
{
  ui->viewWidget->scrollView(500, 0);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionPanUp_triggered(void)
{
  ui->viewWidget->scrollView(0, -500);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionPanDown_triggered(void)
{
  ui->viewWidget->scrollView(0, 500);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionHighlight_toggled(bool checked)
{
  if (m_transition) {
    return;
  }
  m_transition = true;
  ui->actionAreaZoom->setChecked(false);
  ui->actionMousePan->setChecked(false);
  ui->actionMeasure->setChecked(false);
  m_transition = false;
  ui->viewWidget->setHighlightEnabled(checked);
  if (m_activeInfoBox) {
    m_activeInfoBox->layer()->setHighlightEnabled(checked);
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionClearHighlight_triggered(void)
{
  ui->viewWidget->clearHighlight();
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionFeatureProperties_triggered(void)
{
  m_featurePropertiesDialog->show();
}

void ViewerWindow::on_actionMeasure_toggled(bool checked)
{
  if (m_transition) {
    return;
  }
  m_transition = true;
  ui->actionAreaZoom->setChecked(false);
  ui->actionMousePan->setChecked(false);
  ui->actionHighlight->setChecked(false);
  m_transition = false;

  ui->viewWidget->setMeasureEnabled(checked);
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionShowOutline_toggled(bool checked)
{
  for (int i = 0; i < m_visibles.size(); ++i) {
    m_visibles[i]->layer()->setShowOutline(checked);
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionShowStepRepeat_toggled(bool checked)
{
  for (int i = 0; i < m_visibles.size(); ++i) {
    m_visibles[i]->layer()->setShowStepRepeat(checked);
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionShowNotes_toggled(bool checked)
{
  for (int i = 0; i < m_visibles.size(); ++i) {
    if (checked) {
      ui->viewWidget->addItem(m_visibles[i]->layer()->notes());
    } else {
      ui->viewWidget->removeItem(m_visibles[i]->layer()->notes());
    }
  }
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionExportPNG_triggered(void)
{
  LOG_STEP("Export to PNG triggered");
  
  QString defaultFileName = QString("%1_%2").arg(m_job).arg(m_step);
  
  if (!m_visibles.isEmpty()) {
    defaultFileName += "_";
    for (int i = 0; i < qMin(m_visibles.size(), 3); ++i) {
      if (i > 0) {
        defaultFileName += "+";
      }
      defaultFileName += m_visibles[i]->name();
    }
    if (m_visibles.size() > 3) {
      defaultFileName += QString("+%1more").arg(m_visibles.size() - 3);
    }
  }
  
  defaultFileName += ".png";
  
  QString filePath = QFileDialog::getSaveFileName(this, tr("Export to PNG"),                                                                    
      defaultFileName, tr("PNG Files (*.png)"));
  
  if (filePath.isEmpty()) {
    LOG_INFO("PNG export cancelled by user");
    return;
  }
  
  LOG_INFO(QString("Exporting to PNG file: %1").arg(filePath));
  
  if (!filePath.endsWith(".png", Qt::CaseInsensitive)) {
    filePath += ".png";                                                                     
  }
  
  QDialog resDialog(this);
  resDialog.setWindowTitle(tr("Export Resolution"));
  
  QVBoxLayout* layout = new QVBoxLayout(&resDialog);
  
  QLabel* label = new QLabel(tr("Choose PNG resolution:"), &resDialog);
  layout->addWidget(label);
  
  QRadioButton* screenRes = new QRadioButton(tr("Current view size (3x scale)"), &resDialog);
  QRadioButton* fixedRes = new QRadioButton(tr("Fixed size: 20000 x 20000 pixels"), &resDialog);
  QRadioButton* customRes = new QRadioButton(tr("Custom size:"), &resDialog);
  
  screenRes->setChecked(true);
  
  layout->addWidget(screenRes);
  layout->addWidget(fixedRes);
  layout->addWidget(customRes);
  
  QHBoxLayout* customLayout = new QHBoxLayout();
  QSpinBox* widthBox = new QSpinBox(&resDialog);
  QSpinBox* heightBox = new QSpinBox(&resDialog);
  
  widthBox->setRange(100, 32767);
  heightBox->setRange(100, 32767);
  widthBox->setValue(10000);
  heightBox->setValue(10000);
  widthBox->setSuffix(" px");
  heightBox->setSuffix(" px");
  widthBox->setEnabled(false);
  heightBox->setEnabled(false);
  
  customLayout->addWidget(new QLabel(tr("Width:"), &resDialog));
  customLayout->addWidget(widthBox);
  customLayout->addWidget(new QLabel(tr("Height:"), &resDialog));
  customLayout->addWidget(heightBox);
  
  layout->addLayout(customLayout);
  
  QObject::connect(customRes, &QRadioButton::toggled, [widthBox, heightBox](bool checked) {
    widthBox->setEnabled(checked);
    heightBox->setEnabled(checked);
  });
  
  QLabel* warningLabel = new QLabel(tr("Note: Very large images may take significant time to  render and require substantial memory."), &resDialog);
  warningLabel->setWordWrap(true);
  warningLabel->setStyleSheet("color: #FF6600;");
  layout->addWidget(warningLabel);
  
  QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &resDialog);
  layout->addWidget(buttonBox);
  
  QObject::connect(buttonBox, &QDialogButtonBox::accepted, &resDialog, &QDialog::accept);
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, &resDialog, &QDialog::reject);
  
  resDialog.setLayout(layout);
  
  if (resDialog.exec() != QDialog::Accepted) {
    LOG_INFO("PNG export resolution dialog cancelled");
    return;
  }
  
  QMessageBox msg(QMessageBox::Information, "Progress", "Rendering image...");
  msg.setStandardButtons(QMessageBox::NoButton);
  msg.show();
  QApplication::processEvents();
  
  QRect viewRect = ui->viewWidget->viewport()->rect();
  QRectF sceneRect = ui->viewWidget->mapToScene(viewRect).boundingRect();
  
  int imgWidth, imgHeight;
  QRectF targetRect;
  
  if (fixedRes->isChecked()) {
    imgWidth = 20000;
    imgHeight = 20000;
    
    LOG_INFO("Using fixed 20000x20000 resolution");
    
    QRectF adjustedSceneRect = sceneRect;
    double sceneAspect = sceneRect.width() / sceneRect.height();
    double imgAspect = static_cast<double>(imgWidth) / imgHeight;
    
    if (sceneAspect > imgAspect) {
      double newHeight = sceneRect.width() / imgAspect;
      double heightDiff = newHeight - sceneRect.height();
      adjustedSceneRect.adjust(0, -heightDiff/2, 0, heightDiff/2);
    } else {
      double newWidth = sceneRect.height() * imgAspect;
      double widthDiff = newWidth - sceneRect.width();
      adjustedSceneRect.adjust(-widthDiff/2, 0, widthDiff/2, 0);
    }
    
    sceneRect = adjustedSceneRect;
    targetRect = QRectF(0, 0, imgWidth, imgHeight);
    
  } else if (customRes->isChecked()) {
    imgWidth = widthBox->value();
    imgHeight = heightBox->value();
    
    LOG_INFO(QString("Using custom resolution: %1x%2").arg(imgWidth).arg(imgHeight));
    
    QRectF adjustedSceneRect = sceneRect;
    double sceneAspect = sceneRect.width() / sceneRect.height();
    double imgAspect = static_cast<double>(imgWidth) / imgHeight;
    
    if (sceneAspect > imgAspect) {
      double newHeight = sceneRect.width() / imgAspect;
      double heightDiff = newHeight - sceneRect.height();
      adjustedSceneRect.adjust(0, -heightDiff/2, 0, heightDiff/2);
    } else {
      double newWidth = sceneRect.height() * imgAspect;
      double widthDiff = newWidth - sceneRect.width();
      adjustedSceneRect.adjust(-widthDiff/2, 0, widthDiff/2, 0);
    }
    
    sceneRect = adjustedSceneRect;
    targetRect = QRectF(0, 0, imgWidth, imgHeight);
    
  } else {
    int scale = 3;
    imgWidth = viewRect.width() * scale;
    imgHeight = viewRect.height() * scale;
    
    LOG_INFO(QString("Using screen resolution with 3x scale: %1x%2").arg(imgWidth).arg(imgHeight));
    targetRect = QRectF(0, 0, imgWidth, imgHeight);
  }
  
  try {
    LOG_INFO(QString("Creating image with dimensions: %1x%2").arg(imgWidth).arg(imgHeight));
    QImage image(imgWidth, imgHeight, QImage::Format_ARGB32);
    
    if (image.isNull()) {
      throw std::runtime_error("Failed to allocate memory for image");
    }
    
    image.fill(ctx.bg_color);
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    msg.setText(tr("Rendering image (%1x%2)...").arg(imgWidth).arg(imgHeight));
    QApplication::processEvents();
    
    LOG_INFO("Rendering scene to image");
    ui->viewWidget->scene()->render(&painter, targetRect, sceneRect);
    
    msg.setText(tr("Saving PNG file..."));
    QApplication::processEvents();
    
    LOG_INFO("Saving image to file");
    bool success = image.save(filePath, "PNG");
    
    msg.hide();
    
    if (success) {
      LOG_INFO(QString("PNG file saved successfully: %1").arg(filePath));
      QMessageBox::information(this, tr("Export Successful"),
                              tr("Design has been successfully exported to:\n%1\n\nResolution: %2x%3 pixels")
                              .arg(filePath)
                              .arg(imgWidth)
                              .arg(imgHeight));
    } else {
      LOG_ERROR(QString("Failed to save PNG file: %1").arg(filePath));
      QMessageBox::critical(this, tr("Export Failed"),
                           tr("Failed to save the design as PNG file. Please check file permissions."));
    }
  }
  catch (const std::exception& e) {
    LOG_ERROR(QString("Exception during PNG export: %1").arg(e.what()));
    msg.hide();
    QMessageBox::critical(this, tr("Export Failed"),
                         tr("Failed to create the PNG image: %1").arg(e.what()));
  }
  catch (...) {
    LOG_ERROR("Unknown exception during PNG export");
    msg.hide();
    QMessageBox::critical(this, tr("Export Failed"),
                         tr("Failed to create the PNG image due to insufficient memory. Try a smaller resolution."));
  }
  
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

void ViewerWindow::on_actionGoToCoordinate_triggered(void)
{
  m_goToCoordinateDialog->setDisplayUnit(m_displayUnit);
  
  if (m_goToCoordinateDialog->exec() == QDialog::Accepted) {
    QPointF targetCoord = m_goToCoordinateDialog->getCoordinate();
    double zoomLevel = m_goToCoordinateDialog->getZoomLevel();
    
    QString savedFilePath;
    QString detectedObject;
    bool success = navigateAndCapture("", targetCoord.x(), targetCoord.y(), zoomLevel, 
                                     &savedFilePath, nullptr, &detectedObject);
    
    if (success) {
      QString message = tr("Coordinate view has been automatically exported to:\n%1\n\n"
                          "Coordinate: (%2, %3) inches\n"
                          "Zoom: %4x\n"
                          "Detected: %5")
        .arg(savedFilePath)
        .arg(targetCoord.x(), 0, 'f', 3)
        .arg(targetCoord.y(), 0, 'f', 3)
        .arg(zoomLevel)
        .arg(detectedObject);
      
      QMessageBox::information(this, tr("Auto-Export Successful"), message);
    }
  }
}

void ViewerWindow::handleCaptureRequest(const QJsonObject &request)
{
    LOG_INFO("=== REST API Capture Request Received ===");
    qDebug() << request;
    
    QString requestId = request["requestId"].toString();
    QString jobName = request["jobName"].toString();
    QString layerName = request["layerName"].toString();
    double x = request["x"].toDouble();
    double y = request["y"].toDouble();
    double zoom = request["zoom"].toDouble(64.0);
    
    if (jobName.isEmpty()) {
        LOG_ERROR("jobName is empty");
        return;
    }
    
    if (m_job != jobName) {
        LOG_WARNING(QString("Job name mismatch: current=%1, requested=%2")
                   .arg(m_job).arg(jobName));
    }
    
    QTimer::singleShot(100, this, [=]() {
        QString savedFilePath;
        QByteArray imageData;
        QString detectedObject;
        
        bool success = navigateAndCapture(layerName, x, y, zoom, 
                                         &savedFilePath, &imageData, &detectedObject);
        
        if (!success) {
            LOG_ERROR("Failed to navigate and capture image");
            return;
        }
        
        LOG_INFO(QString("Capture successful: %1 bytes, saved to %2, detected: %3")
                .arg(imageData.size()).arg(savedFilePath).arg(detectedObject));
        
        QJsonObject metadata;
        metadata["requestId"] = requestId;
        metadata["jobName"] = m_job;
        metadata["layerName"] = layerName;
        metadata["x"] = x;
        metadata["y"] = y;
        metadata["zoom"] = zoom;
        metadata["imageSize"] = imageData.size();
        metadata["format"] = "PNG";
        metadata["savedPath"] = savedFilePath;
        metadata["detectedObject"] = detectedObject;
        metadata["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        if (m_restApiServer) {
            m_restApiServer->sendCaptureResponse(requestId, imageData, metadata);
            LOG_INFO("Capture response sent to client");
        }
    });
}

void ViewerWindow::startRestApiServer(quint16 port)
{
    if (m_restApiServer) {
        delete m_restApiServer;
        m_restApiServer = nullptr;
    }
    
    m_restApiServer = new RestApiServer(port, this);
    
    if (m_restApiServer->isListening()) {
        qDebug() << "REST API server started on port" << port;
        LOG_INFO(QString("REST API server started on port %1").arg(port));
        
        connect(m_restApiServer, &RestApiServer::captureRequest,
                this, &ViewerWindow::handleCaptureRequest);
    } else {
        qDebug() << "Failed to start REST API server on port" << port;
        LOG_ERROR(QString("Failed to start REST API server on port %1").arg(port));
    }
}

void ViewerWindow::on_actionSaveHighlight_triggered()
{
  if (!m_activeInfoBox || !m_activeInfoBox->layer()) {
    QMessageBox::warning(this, tr("No Active Layer"),
                        tr("Please select an active layer first."));
    return;
  }
  
  GraphicsLayerScene* scene = dynamic_cast<GraphicsLayerScene*>(
    m_activeInfoBox->layer()->layerScene());
  
  if (!scene) {
    QMessageBox::warning(this, tr("Invalid Scene"),
                        tr("Cannot access layer scene."));
    return;
  }
  
  QJsonObject data = scene->exportHighlightData();
  
  if (data["highlightCount"].toInt() == 0) {
    QMessageBox::information(this, tr("No Highlights"),
                            tr("No highlighted symbols to save."));
    return;
  }
  
  QString defaultName = QString("%1_%2_%3_highlights.json")
    .arg(m_job)
    .arg(m_step)
    .arg(m_activeInfoBox->name());
  
  QString filePath = QFileDialog::getSaveFileName(
    this,
    tr("Save Highlights"),
    defaultName,
    tr("JSON Files (*.json);;All Files (*)")
  );
  
  if (filePath.isEmpty()) {
    return;
  }
  
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox::critical(this, tr("Save Failed"),
                         tr("Cannot write to file:\n%1").arg(filePath));
    return;
  }
  
  QJsonDocument doc(data);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();
  
  QMessageBox::information(this, tr("Save Successful"),
                          tr("Highlights saved to:\n%1\n\n"
                             "Symbols saved: %2")
                          .arg(filePath)
                          .arg(data["highlightCount"].toInt()));
  
  LOG_INFO(QString("Highlights saved to: %1").arg(filePath));
}

void ViewerWindow::on_actionLoadHighlight_triggered()
{
  if (!m_activeInfoBox || !m_activeInfoBox->layer()) {
    QMessageBox::warning(this, tr("No Active Layer"),
                        tr("Please select an active layer first."));
    return;
  }
  
  GraphicsLayerScene* scene = dynamic_cast<GraphicsLayerScene*>(
    m_activeInfoBox->layer()->layerScene());
  
  if (!scene) {
    QMessageBox::warning(this, tr("Invalid Scene"),
                        tr("Cannot access layer scene."));
    return;
  }
  
  QString filePath = QFileDialog::getOpenFileName(
    this,
    tr("Load Highlights"),
    QString(),
    tr("JSON Files (*.json);;All Files (*)")
  );
  
  if (filePath.isEmpty()) {
    return;
  }
  
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::critical(this, tr("Load Failed"),
                         tr("Cannot read file:\n%1").arg(filePath));
    return;
  }
  
  QByteArray data = file.readAll();
  file.close();
  
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (doc.isNull() || !doc.isObject()) {
    QMessageBox::critical(this, tr("Load Failed"),
                         tr("Invalid JSON format in file:\n%1").arg(filePath));
    return;
  }
  
  QJsonObject jsonObj = doc.object();
  bool success = scene->importHighlightData(jsonObj);
  
  if (success) {
    int count = jsonObj["highlightCount"].toInt();
    QMessageBox::information(this, tr("Load Successful"),
                            tr("Highlights loaded from:\n%1\n\n"
                               "Symbols loaded: %2")
                            .arg(filePath)
                            .arg(count));
    LOG_INFO(QString("Highlights loaded from: %1").arg(filePath));
  } else {
    QMessageBox::warning(this, tr("Load Partially Failed"),
                        tr("Some highlights could not be loaded.\n"
                           "Check that you're using the correct layer."));
  }
}

void ViewerWindow::on_actionSelectTraceR1_triggered()
{
  if (m_activeInfoBox && m_activeInfoBox->layer()) {
    GraphicsLayerScene* scene = dynamic_cast<GraphicsLayerScene*>(
        m_activeInfoBox->layer()->layerScene());
    
    if (scene) {
      scene->selectTracesR1();
      qDebug() << "Selected traces with width < 6 mils";
    }
  }
}

void ViewerWindow::on_actionSelectTraceR2_triggered()
{
  if (m_activeInfoBox && m_activeInfoBox->layer()) {
    GraphicsLayerScene* scene = dynamic_cast<GraphicsLayerScene*>(
        m_activeInfoBox->layer()->layerScene());
    
    if (scene) {
      scene->selectTracesR2();
      qDebug() << "Selected traces with width < 10 mils";
    }
  }
}

void ViewerWindow::on_actionSelectTraceR3_triggered()
{
  if (m_activeInfoBox && m_activeInfoBox->layer()) {
    GraphicsLayerScene* scene = dynamic_cast<GraphicsLayerScene*>(
        m_activeInfoBox->layer()->layerScene());
    
    if (scene) {
      scene->selectTracesR3();
      qDebug() << "Selected traces with width < 15 mils";
    }
  }
}

void ViewerWindow::on_actionToggleHighlightColor_triggered()
{
  if (m_highlightColor == QColor(0, 0, 255)) {
    m_highlightColor = QColor(179, 0, 255);
    ctx.highlight_color = QColor(179, 0, 255);
    m_highlightColorButton->setStyleSheet("QPushButton { background-color: rgb(179, 0, 255); color: white; }");
    qDebug() << "Highlight color changed to PURPLE RGB(179, 0, 255)";
  } else {
    m_highlightColor = QColor(0, 0, 255);
    ctx.highlight_color = QColor(0, 0, 255);
    m_highlightColorButton->setStyleSheet("QPushButton { background-color: rgb(0, 0, 255); color: white; }");
    qDebug() << "Highlight color changed to BLUE RGB(0, 0, 255)";
  }
  
  ui->viewWidget->setFocus(Qt::MouseFocusReason);
}

QColor ViewerWindow::getHighlightColor() const
{
  return m_highlightColor;
}

QString ViewerWindow::detectObjectAtCoordinate(const QImage &image, const QPointF &sceneCoord, 
                                                const QRectF &sceneRect, const QRectF &targetRect)
{
    double scaleX = targetRect.width() / sceneRect.width();
    double scaleY = targetRect.height() / sceneRect.height();
    
    int imgX = static_cast<int>((sceneCoord.x() - sceneRect.left()) * scaleX);
    int imgY = static_cast<int>((sceneCoord.y() - sceneRect.top()) * scaleY);
    
    LOG_INFO(QString("Detecting color at scene(%1, %2) -> pixel(%3, %4)")
             .arg(sceneCoord.x()).arg(sceneCoord.y()).arg(imgX).arg(imgY));
    
    if (imgX < 0 || imgX >= image.width() || imgY < 0 || imgY >= image.height()) {
        LOG_WARNING(QString("Coordinate out of bounds: pixel(%1, %2), image size(%3, %4)")
                   .arg(imgX).arg(imgY).arg(image.width()).arg(image.height()));
        return "unknown";
    }
    
    QMap<QRgb, int> colorCount;
    int sampleSize = 5;
    int halfSize = sampleSize / 2;
    
    for (int dy = -halfSize; dy <= halfSize; ++dy) {
        for (int dx = -halfSize; dx <= halfSize; ++dx) {
            int px = imgX + dx;
            int py = imgY + dy;
            
            if (px >= 0 && px < image.width() && py >= 0 && py < image.height()) {
                QRgb rgb = image.pixel(px, py);
                colorCount[rgb]++;
            }
        }
    }
    
    QRgb bgRgb = ctx.bg_color.rgb();
    QRgb dominantColor = bgRgb;
    int maxCount = 0;
    
    for (auto it = colorCount.begin(); it != colorCount.end(); ++it) {
        if (it.value() > maxCount && it.key() != bgRgb) {
            dominantColor = it.key();
            maxCount = it.value();
        }
    }
    
    QColor detectedColor(dominantColor);
    LOG_INFO(QString("Dominant color detected: RGB(%1, %2, %3), count: %4")
             .arg(detectedColor.red()).arg(detectedColor.green()).arg(detectedColor.blue()).arg(maxCount));
    
    auto colorMatch = [](const QColor &c1, const QColor &c2, int tolerance = 30) -> bool {
        return qAbs(c1.red() - c2.red()) <= tolerance &&
               qAbs(c1.green() - c2.green()) <= tolerance &&
               qAbs(c1.blue() - c2.blue()) <= tolerance;
    };
    
    if (dominantColor == bgRgb || colorMatch(detectedColor, Qt::black, 10)) {
        LOG_INFO("Detected: NONE (background/black)");
        return "none";
    }
    
    if (colorMatch(detectedColor, QColor(0, 0, 255), 40)) {
        LOG_INFO("Detected: BETA COOPER (blue highlight)");
        return "beta_cooper";
    }
    
    if (colorMatch(detectedColor, QColor(179, 0, 255), 40)) {
        LOG_INFO("Detected: TRACE (purple highlight)");
        return "trace";
    }
    
    LOG_INFO(QString("Detected: UNKNOWN (unrecognized color RGB(%1, %2, %3))")
             .arg(detectedColor.red()).arg(detectedColor.green()).arg(detectedColor.blue()));
    return "unknown";
}

bool ViewerWindow::navigateAndCapture(const QString &layerName, double x, double y, double zoom,
                                     QString *outputPath, QByteArray *imageData, QString *detectedObject)
{
    LOG_INFO(QString("navigateAndCapture: layer=%1, x=%2, y=%3, zoom=%4")
             .arg(layerName).arg(x).arg(y).arg(zoom));
    
    LayerInfoBox* targetLayer = nullptr;
    if (!layerName.isEmpty()) {
        if (m_SelectorMap.contains(layerName)) {
            targetLayer = m_SelectorMap[layerName];
            if (targetLayer) {
                LOG_INFO(QString("Found layer: %1").arg(layerName));
                
                if (targetLayer->layer() == nullptr) {
                    LOG_ERROR(QString("Layer %1 exists but has no layer data!").arg(layerName));
                    return false;
                }
                
                bool isVisible = m_visibles.contains(targetLayer);
                if (!isVisible) {
                    LOG_INFO(QString("Layer %1 is not visible, toggling ON...").arg(layerName));
                    targetLayer->toggle();
                    
                    QApplication::processEvents();
                    QThread::msleep(100);
                }
                
                targetLayer->setActive(true);
                LOG_INFO(QString("Layer %1 set as active").arg(layerName));
                
                QApplication::processEvents();
                QThread::msleep(50);
            }
        } else {
            LOG_ERROR(QString("Layer not found in m_SelectorMap: %1").arg(layerName));
            return false;
        }
    } else {
        LOG_INFO("No layer name specified, using current active layer");
        targetLayer = m_activeInfoBox;
    }
    
    QPointF targetCoord(x, y);
    QPointF sceneCoord(x, -y);
    
    LOG_INFO(QString("Centering view on coordinate: (%1, %2) inches -> scene(%3, %4)")
             .arg(x).arg(y).arg(sceneCoord.x()).arg(sceneCoord.y()));
    ui->viewWidget->centerOn(sceneCoord);
    
    LOG_INFO(QString("Applying absolute zoom: %1x").arg(zoom));
    ui->viewWidget->setAbsoluteZoom(zoom);
    ui->viewWidget->setFocus(Qt::MouseFocusReason);
    
    ui->viewWidget->scene()->update();
    QApplication::processEvents();
    QThread::msleep(150);
    
    QRect viewRect = ui->viewWidget->viewport()->rect();
    QRectF sceneRect = ui->viewWidget->mapToScene(viewRect).boundingRect();
    int scale = 3;
    int imgWidth = viewRect.width() * scale;
    int imgHeight = viewRect.height() * scale;
    QRectF targetRect(0, 0, imgWidth, imgHeight);
    
    LOG_INFO(QString("Creating image with dimensions: %1x%2").arg(imgWidth).arg(imgHeight));
    QImage image(imgWidth, imgHeight, QImage::Format_ARGB32);
    
    if (image.isNull()) {
        LOG_ERROR("Failed to allocate memory for image");
        return false;
    }
    
    image.fill(ctx.bg_color);
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    LOG_INFO("Rendering scene to image");
    ui->viewWidget->scene()->render(&painter, targetRect, sceneRect);
    
    painter.end();
    
    QString objectType = "none";
    double traceWidth = -1.0;
    double traceAngle = 0.0;
    
    if (detectedObject) {
        objectType = detectObjectAtCoordinate(image, sceneCoord, sceneRect, targetRect);
        LOG_INFO(QString("Object detection result: %1").arg(objectType));
        
        // ========================================
        // NEW: If trace detected, measure width
        // ========================================
        if (objectType == "trace") {
            LOG_INFO("Trace detected, attempting to get angle from Symbol...");
            
            Symbol* foundSymbol = nullptr;
            
            if (targetLayer && targetLayer->layer()) {
                GraphicsLayerScene* layerScene = dynamic_cast<GraphicsLayerScene*>(
                    targetLayer->layer()->layerScene());
                
                if (layerScene) {
                    QList<QGraphicsItem*> itemsAtPoint = layerScene->items(sceneCoord);
                    
                    LOG_INFO(QString("Found %1 items at coordinate").arg(itemsAtPoint.size()));
                    
                    for (int i = 0; i < itemsAtPoint.size(); ++i) {
                        QGraphicsItem* item = itemsAtPoint[i];
                        
                        Symbol* sym = dynamic_cast<Symbol*>(item);
                        if (sym) {
                            foundSymbol = sym;
                            
                            QString symbolInfo = sym->infoText();
                            LOG_INFO(QString("Found Symbol: %1").arg(symbolInfo));
                            
                            // Get angle from Symbol
                            qreal symAngle = sym->getAngle();
                            if (symAngle >= 0.0) {
                                traceAngle = symAngle;
                                LOG_INFO(QString("Symbol angle: %1").arg(traceAngle));
                            } else {
                                LOG_WARNING("Symbol->getAngle() returned invalid value");
                                traceAngle = 0.0;
                            }
                            break;
                        }
                    }
                    
                    if (!foundSymbol) {
                        LOG_WARNING("No Symbol found at coordinate - will use angle=0");
                        traceAngle = 0.0;
                    }
                } else {
                    LOG_WARNING("Cannot access layer scene");
                    traceAngle = 0.0;
                }
            } else {
                LOG_WARNING("No target layer available");
                traceAngle = 0.0;
            }
            
            // Measure width with NEW METHOD
            LOG_INFO(QString("Measuring width with angle=%1").arg(traceAngle));
            
            traceWidth = measureTraceWidthImproved(image, sceneCoord, sceneRect, targetRect, traceAngle);
            
            if (traceWidth > 0.0 && traceWidth < 50.0) {
                double widthMils = traceWidth / 0.0254;
                LOG_INFO(QString("Trace width measured: %1 mm (%2 mils) at angle %3")
                         .arg(traceWidth, 0, 'f', 4)
                         .arg(widthMils, 0, 'f', 2)
                         .arg(traceAngle));
                
                objectType = QString("trace_%1mm_%2mils_angle%3deg")
                            .arg(traceWidth, 0, 'f', 3)
                            .arg(widthMils, 0, 'f', 1)
                            .arg(traceAngle, 0, 'f', 0);
            } else {
                LOG_WARNING(QString("Measurement failed or unreasonable: %1 mm").arg(traceWidth));
                objectType = "trace_measurement_failed";
            }
        }
        
        *detectedObject = objectType;
    }
    
    QString exportDir = "C:/Users/Admin/Desktop/Export";
    QDir dir;
    if (!dir.exists(exportDir)) {
        if (!dir.mkpath(exportDir)) {
            LOG_ERROR("Failed to create export directory: " + exportDir);
            return false;
        }
    }
    
    QString coordStr = QString("_at_%1_%2")
                       .arg(targetCoord.x(), 0, 'f', 3)
                       .arg(targetCoord.y(), 0, 'f', 3);
    
    QString filename = QString("%1_%2%3%4_%5")
        .arg(m_job).arg(m_step).arg(layerName).arg(coordStr).arg(objectType);
    filename += ".png";
    QString filePath = exportDir + "/" + filename;
    
    LOG_INFO(QString("Saving image to: %1").arg(filePath));
    
    if (outputPath) {
        LOG_INFO("Saving image to file");
        if (!image.save(filePath, "PNG")) {
            LOG_ERROR(QString("Failed to save PNG file: %1").arg(filePath));
            return false;
        }
        *outputPath = filePath;
        LOG_INFO(QString("PNG file saved successfully: %1").arg(filePath));
    }
    
    if (imageData) {
        QBuffer buffer(imageData);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) {
            LOG_ERROR("Failed to convert image to byte array");
            return false;
        }
        LOG_INFO(QString("Image converted to byte array: %1 bytes").arg(imageData->size()));
    }
    
    return true;
}

double ViewerWindow::measureTraceWidth(const QImage &image, const QPointF &sceneCoord,
                                       const QRectF &sceneRect, const QRectF &targetRect,
                                       double angle)
{
    double scaleX = targetRect.width() / sceneRect.width();
    double scaleY = targetRect.height() / sceneRect.height();
    
    int imgX = static_cast<int>((sceneCoord.x() - sceneRect.left()) * scaleX);
    int imgY = static_cast<int>((sceneCoord.y() - sceneRect.top()) * scaleY);
    
    LOG_INFO(QString("Measuring trace width at pixel(%1, %2), angle=%3")
             .arg(imgX).arg(imgY).arg(angle));
    
    if (imgX < 0 || imgX >= image.width() || imgY < 0 || imgY >= image.height()) {
        LOG_WARNING(QString("Coordinate out of bounds: pixel(%1,%2), image(%3x%4)")
                   .arg(imgX).arg(imgY).arg(image.width()).arg(image.height()));
        return -1.0;
    }
    
    // Normalize angle to [0, 180)
    double normalizedAngle = std::fmod(angle, 180.0);
    if (normalizedAngle < 0) {
        normalizedAngle += 180.0;
    }
    
    LOG_INFO(QString("Normalized angle: %1").arg(normalizedAngle));
    
    QRgb bgRgb = ctx.bg_color.rgb();
    
    auto isTraceColor = [bgRgb](QRgb rgb) -> bool {
        // Check if not background and not black
        if (rgb == bgRgb) {
            return false;
        }
        if (qRed(rgb) < 30 && qGreen(rgb) < 30 && qBlue(rgb) < 30) {
            return false;
        }
        return true;
    };
    
    double l1 = 0.0, l2 = 0.0;
    
    // Determine measurement direction based on angle
    if (normalizedAngle < 1.0 || normalizedAngle > 179.0) {
        // Angle ≈ 0° or 180°: measure horizontally
        LOG_INFO("Measuring horizontally (angle ≈ 0/180)");
        
        // Measure l1 (to the left)
        for (int dx = 1; dx <= image.width(); ++dx) {
            int px = imgX - dx;
            if (px < 0) {
                l1 = dx - 1;
                break;
            }
            
            if (!isTraceColor(image.pixel(px, imgY))) {
                l1 = dx - 1;
                break;
            }
        }
        
        // Measure l2 (to the right)
        for (int dx = 1; dx <= image.width(); ++dx) {
            int px = imgX + dx;
            if (px >= image.width()) {
                l2 = dx - 1;
                break;
            }
            
            if (!isTraceColor(image.pixel(px, imgY))) {
                l2 = dx - 1;
                break;
            }
        }
        
        double widthPixels = l1 + l2 + 1;
        LOG_INFO(QString("Horizontal measurement: l1=%1, l2=%2, total=%3 pixels")
                 .arg(l1).arg(l2).arg(widthPixels));
        
        if (widthPixels <= 1) {
            LOG_WARNING("Width too small or measurement failed");
            return -1.0;
        }
        
        double pixelsPerInch = scaleX;
        double widthInches = widthPixels / pixelsPerInch;
        double widthMM = widthInches * 25.4;
        
        LOG_INFO(QString("Conversion: %1 px -> %2 inches -> %3 mm")
                 .arg(widthPixels).arg(widthInches).arg(widthMM));
        
        return widthMM;
        
    } else {
        // Other angles: measure vertically
        LOG_INFO(QString("Measuring vertically (angle = %1°)").arg(normalizedAngle));
        
        // Measure l1 (upward)
        for (int dy = 1; dy <= image.height(); ++dy) {
            int py = imgY - dy;
            if (py < 0) {
                l1 = dy - 1;
                break;
            }
            
            if (!isTraceColor(image.pixel(imgX, py))) {
                l1 = dy - 1;
                break;
            }
        }
        
        // Measure l2 (downward)
        for (int dy = 1; dy <= image.height(); ++dy) {
            int py = imgY + dy;
            if (py >= image.height()) {
                l2 = dy - 1;
                break;
            }
            
            if (!isTraceColor(image.pixel(imgX, py))) {
                l2 = dy - 1;
                break;
            }
        }
        
        double widthPixels = l1 + l2 + 1;
        LOG_INFO(QString("Vertical measurement: l1=%1, l2=%2, total=%3 pixels")
                 .arg(l1).arg(l2).arg(widthPixels));
        
        if (widthPixels <= 1) {
            LOG_WARNING("Width too small or measurement failed");
            return -1.0;
        }
        
        double pixelsPerInch = scaleY;
        double widthInches = widthPixels / pixelsPerInch;
        double widthMM = widthInches * 25.4;
        
        // Apply cosine correction for angled traces
        double angleRad = normalizedAngle * M_PI / 180.0;
        double cosAngle = std::cos(angleRad);
        widthMM *= std::abs(cosAngle);
        
        LOG_INFO(QString("Conversion: %1 px -> %2 inches -> %3 mm (before cos)")
                 .arg(widthPixels).arg(widthInches).arg(widthInches * 25.4));
        LOG_INFO(QString("Width corrected by cos(%1°) = %2, final = %3 mm")
                 .arg(normalizedAngle).arg(cosAngle).arg(widthMM));
        
        return widthMM;
    }
}

double ViewerWindow::measureTraceWidthImproved(const QImage &image, const QPointF &sceneCoord,
                                               const QRectF &sceneRect, const QRectF &targetRect,
                                               double angle)
{
    double scaleX = targetRect.width() / sceneRect.width();
    double scaleY = targetRect.height() / sceneRect.height();
    
    int imgX = static_cast<int>((sceneCoord.x() - sceneRect.left()) * scaleX);
    int imgY = static_cast<int>((sceneCoord.y() - sceneRect.top()) * scaleY);
    
    LOG_INFO(QString("=== NEW MEASUREMENT METHOD ==="));
    LOG_INFO(QString("Measuring at pixel(%1, %2), angle=%3°")
             .arg(imgX).arg(imgY).arg(angle));
    LOG_INFO(QString("Scale: scaleX=%1 px/inch, scaleY=%2 px/inch")
             .arg(scaleX).arg(scaleY));
    
    if (imgX < 0 || imgX >= image.width() || imgY < 0 || imgY >= image.height()) {
        LOG_WARNING("Coordinate out of bounds");
        return -1.0;
    }
    
    // Normalize angle to [0, 360)
    double normalizedAngle = std::fmod(angle, 360.0);
    if (normalizedAngle < 0) {
        normalizedAngle += 360.0;
    }
    
    LOG_INFO(QString("Normalized angle: %1° (original: %2°)").arg(normalizedAngle).arg(angle));
    
    QRgb bgRgb = ctx.bg_color.rgb();
    QRgb centerPixel = image.pixel(imgX, imgY);
    QColor centerColor(centerPixel);
    
    LOG_INFO(QString("Center pixel: RGB(%1, %2, %3)")
             .arg(centerColor.red()).arg(centerColor.green()).arg(centerColor.blue()));
    
    // Validate trace color
    bool isPurple = (centerColor.red() > 150 && centerColor.blue() > 200 && centerColor.green() < 50);
    bool isBlue = (centerColor.blue() > 200 && centerColor.red() < 100 && centerColor.green() < 100);
    
    if (!isPurple && !isBlue) {
        LOG_WARNING(QString("Not trace color: RGB(%1,%2,%3)")
                   .arg(centerColor.red()).arg(centerColor.green()).arg(centerColor.blue()));
        return -1.0;
    }
    
    auto isTraceColor = [bgRgb, centerPixel](QRgb rgb) -> bool {
        if (rgb == bgRgb) return false;
        
        QColor c1(rgb);
        QColor c2(centerPixel);
        
        int colorDiff = qAbs(c1.red() - c2.red()) + 
                       qAbs(c1.green() - c2.green()) + 
                       qAbs(c1.blue() - c2.blue());
        
        return colorDiff < 30;
    };
    
    int maxSearch = 500;
    
    // ========================================
    // STEP 1: Đo NGANG (Horizontal) → l1, l2
    // ========================================
    double l1 = 0.0, l2 = 0.0;
    
    // Measure l1 (left)
    for (int dx = 1; dx <= maxSearch; ++dx) {
        int px = imgX - dx;
        if (px < 0) break;
        
        if (isTraceColor(image.pixel(px, imgY))) {
            l1 = dx;
        } else {
            break;
        }
    }
    
    // Measure l2 (right)
    for (int dx = 1; dx <= maxSearch; ++dx) {
        int px = imgX + dx;
        if (px >= image.width()) break;
        
        if (isTraceColor(image.pixel(px, imgY))) {
            l2 = dx;
        } else {
            break;
        }
    }
    
    double horizontalSpan = l1 + l2;
    LOG_INFO(QString("HORIZONTAL: l1=%1 (left), l2=%2 (right), total=%3 px")
             .arg(l1).arg(l2).arg(horizontalSpan));
    
    // ========================================
    // STEP 2: Đo DỌC (Vertical) → l3, l4
    // ========================================
    double l3 = 0.0, l4 = 0.0;
    
    // Measure l3 (up)
    for (int dy = 1; dy <= maxSearch; ++dy) {
        int py = imgY - dy;
        if (py < 0) break;
        
        if (isTraceColor(image.pixel(imgX, py))) {
            l3 = dy;
        } else {
            break;
        }
    }
    
    // Measure l4 (down)
    for (int dy = 1; dy <= maxSearch; ++dy) {
        int py = imgY + dy;
        if (py >= image.height()) break;
        
        if (isTraceColor(image.pixel(imgX, py))) {
            l4 = dy;
        } else {
            break;
        }
    }
    
    double verticalSpan = l3 + l4;
    LOG_INFO(QString("VERTICAL: l3=%1 (up), l4=%2 (down), total=%3 px")
             .arg(l3).arg(l4).arg(verticalSpan));
    
    // ========================================
    // STEP 3: Kiểm tra góc và tính width
    // ========================================
    
    // Check if trace is horizontal/vertical (angle = 0, 90, 180, 270, 360)
    bool isCardinal = false;
    if (qAbs(normalizedAngle) < 1.0 || 
        qAbs(normalizedAngle - 90.0) < 1.0 || 
        qAbs(normalizedAngle - 180.0) < 1.0 || 
        qAbs(normalizedAngle - 270.0) < 1.0 || 
        qAbs(normalizedAngle - 360.0) < 1.0) {
        isCardinal = true;
    }
    
    double widthPixels = 0.0;
    
    if (isCardinal) {
        // ========================================
        // CASE 1: Trace ngang/dọc (0°, 90°, 180°, 270°, 360°)
        // w = min(l1+l2, l3+l4)
        // ========================================
        widthPixels = (horizontalSpan < verticalSpan) ? horizontalSpan : verticalSpan;
        
        LOG_INFO(QString("CARDINAL ANGLE (0/90/180/270/360): w = min(%1, %2) = %3 px")
                 .arg(horizontalSpan).arg(verticalSpan).arg(widthPixels));
        
    } else {
        // ========================================
        // CASE 2: Trace nghiêng
        // w = h1 + h2
        // 1/h1² = 1/l1² + 1/l4²  →  h1 = 1/√(1/l1² + 1/l4²)
        // 1/h2² = 1/l2² + 1/l3²  →  h2 = 1/√(1/l2² + 1/l3²)
        // ========================================
        
        double h1 = 0.0;
        double h2 = 0.0;
        
        // Calculate h1: 1/h1² = 1/l1² + 1/l4²
        if (l1 > 0.0 && l4 > 0.0) {
            double inv_h1_squared = (1.0 / (l1 * l1)) + (1.0 / (l4 * l4));
            h1 = 1.0 / std::sqrt(inv_h1_squared);
        } else if (l1 > 0.0) {
            h1 = l1 / 2.0;  // Only l1 side
        } else if (l4 > 0.0) {
            h1 = l4 / 2.0;  // Only l4 side
        } else {
            h1 = 0.0;
        }
        
        // Calculate h2: 1/h2² = 1/l2² + 1/l3²
        if (l2 > 0.0 && l3 > 0.0) {
            double inv_h2_squared = (1.0 / (l2 * l2)) + (1.0 / (l3 * l3));
            h2 = 1.0 / std::sqrt(inv_h2_squared);
        } else if (l2 > 0.0) {
            h2 = l2 / 2.0;  // Only l2 side
        } else if (l3 > 0.0) {
            h2 = l3 / 2.0;  // Only l3 side
        } else {
            h2 = 0.0;
        }
        
        widthPixels = h1 + h2;
        
        LOG_INFO(QString("ANGLED TRACE (%1):").arg(normalizedAngle));
        LOG_INFO(QString("  1/h1² = 1/%1² + 1/%2² → h1 = %3 px").arg(l1).arg(l4).arg(h1));
        LOG_INFO(QString("  1/h2² = 1/%1² + 1/%2² → h2 = %3 px").arg(l2).arg(l3).arg(h2));
        LOG_INFO(QString("  w = h1 + h2 = %1 + %2 = %3 px")
                 .arg(h1).arg(h2).arg(widthPixels));
    }
    
    // ========================================
    // STEP 4: Validation
    // ========================================
    if (widthPixels <= 1.0) {
        LOG_WARNING("Width too small - measurement failed");
        return -1.0;
    }
    
    if (widthPixels >= maxSearch) {
        LOG_ERROR(QString("Width (%1 px) hit maxSearch - unreliable!").arg(widthPixels));
        return -1.0;
    }
    
    // ========================================
    // STEP 5: Convert to mm
    // ========================================
    // Use average scale for conversion
    double pixelsPerInch = (scaleX + scaleY) / 2.0;
    double widthInches = widthPixels / pixelsPerInch;
    double widthMM = widthInches * 25.4;
    
    LOG_INFO(QString("RESULT: %1 px → %2 inches → %3 mm")
             .arg(widthPixels, 0, 'f', 2)
             .arg(widthInches, 0, 'f', 6)
             .arg(widthMM, 0, 'f', 4));
    
    if (widthMM < 0.05 || widthMM > 50.0) {
        LOG_WARNING(QString("Width %1 mm seems unreasonable").arg(widthMM));
    }
    
    return widthMM;
}
