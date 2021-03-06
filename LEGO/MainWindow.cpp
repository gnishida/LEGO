#include "MainWindow.h"
#include <iostream>
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>
#include "AllOptionDialog.h"
#include "DPOptionDialog.h"
#include "RightAngleOptionDialog.h"
#include "CurveOptionDialog.h"
#include "CurveRightAngleOptionDialog.h"
#include "OffsetScaleDialog.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
	ui.setupUi(this);

	// group for simplification modes
	QActionGroup* groupSimplify = new QActionGroup(this);
	groupSimplify->addAction(ui.actionInputVoxel);
	groupSimplify->addAction(ui.actionSimplifyByAll);
	groupSimplify->addAction(ui.actionSimplifyByDP);
	groupSimplify->addAction(ui.actionSimplifyByRightAngle);
	groupSimplify->addAction(ui.actionSimplifyByCurve);
	groupSimplify->addAction(ui.actionSimplifyByCurveRightAngle);

	// group for rendering modes
	QActionGroup* groupColoring = new QActionGroup(this);
	groupColoring->addAction(ui.actionColor);
	groupColoring->addAction(ui.actionTexture);

	// group for rendering modes
	QActionGroup* groupRendering = new QActionGroup(this);
	groupRendering->addAction(ui.actionRenderingBasic);
	groupRendering->addAction(ui.actionRenderingSSAO);
	groupRendering->addAction(ui.actionRenderingHatching);

	connect(ui.actionOpen, SIGNAL(triggered()), this, SLOT(onOpen()));
	connect(ui.actionSaveOBJ, SIGNAL(triggered()), this, SLOT(onSaveOBJ()));
	connect(ui.actionSaveTopFaces, SIGNAL(triggered()), this, SLOT(onSaveTopFaces()));
	connect(ui.actionSavePLY, SIGNAL(triggered()), this, SLOT(onSavePLY()));
	connect(ui.actionSaveImage, SIGNAL(triggered()), this, SLOT(onSaveImage()));
	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(close()));
	connect(ui.actionInputVoxel, SIGNAL(triggered()), this, SLOT(onInputVoxel()));
	connect(ui.actionSimplifyByAll, SIGNAL(triggered()), this, SLOT(onSimplifyByAll()));
	connect(ui.actionSimplifyByDP, SIGNAL(triggered()), this, SLOT(onSimplifyByDP()));
	connect(ui.actionSimplifyByRightAngle, SIGNAL(triggered()), this, SLOT(onSimplifyByRightAngle()));
	connect(ui.actionSimplifyByCurve, SIGNAL(triggered()), this, SLOT(onSimplifyByCurve()));
	connect(ui.actionSimplifyByCurveRightAngle, SIGNAL(triggered()), this, SLOT(onSimplifyByCurveRightAngle()));
	connect(ui.actionOffsetScale, SIGNAL(triggered()), this, SLOT(onOffsetScale()));
	connect(ui.actionColor, SIGNAL(triggered()), this, SLOT(onColoringModeChanged()));
	connect(ui.actionTexture, SIGNAL(triggered()), this, SLOT(onColoringModeChanged()));
	connect(ui.actionRenderingBasic, SIGNAL(triggered()), this, SLOT(onRenderingModeChanged()));
	connect(ui.actionRenderingSSAO, SIGNAL(triggered()), this, SLOT(onRenderingModeChanged()));
	connect(ui.actionRenderingHatching, SIGNAL(triggered()), this, SLOT(onRenderingModeChanged()));

	// create tool bar for file menu
	ui.mainToolBar->addAction(ui.actionOpen);
	ui.mainToolBar->addAction(ui.actionSaveOBJ);

	// setup the GL widget
	glWidget = new GLWidget3D(this);
	setCentralWidget(glWidget);
}

MainWindow::~MainWindow() {
}

void MainWindow::onOpen() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Load voxel data..."), "", tr("Image files (*.png *.jpg *.bmp)"));
	if (filename.isEmpty()) return;

	setWindowTitle("LEGO - " + filename);
	glWidget->loadVoxelData(filename);
	glWidget->update();
}

void MainWindow::onSaveOBJ() {
	QString filename = QFileDialog::getSaveFileName(this, tr("Save OBJ file..."), "", tr("OBJ files (*.obj)"));
	if (!filename.isEmpty()) {
		glWidget->saveOBJ(filename);
	}
}

void MainWindow::onSaveTopFaces() {
	if (glWidget->show_mode == GLWidget3D::SHOW_INPUT) {
		QMessageBox msg;
		msg.setText("Simplify the buildings.");
		msg.exec();
		return;
	}

	QString filename = QFileDialog::getSaveFileName(this, tr("Save text file..."), "", tr("text files (*.txt)"));
	if (!filename.isEmpty()) {
		glWidget->saveTopFace(filename);
	}
}

void MainWindow::onSavePLY() {
	if (glWidget->show_mode == GLWidget3D::SHOW_INPUT) {
		QMessageBox msg;
		msg.setText("Simplify the buildings.");
		msg.exec();
		return;
	}

	QString filename = QFileDialog::getSaveFileName(this, tr("Save PLY file..."), "", tr("PLY files (*.ply)"));
	if (!filename.isEmpty()) {
		glWidget->savePLY(filename);
	}
}

void MainWindow::onSaveImage() {
	if (!QDir("screenshot").exists()) {
		QDir().mkdir("screenshot");
	}
	QDateTime dateTime = QDateTime().currentDateTime();
	QString str = QString("screenshot/") + dateTime.toString("yyyyMMddhhmmss") + QString(".png");

	glWidget->saveImage(str);
}

void MainWindow::onInputVoxel() {
	glWidget->showInputVoxel();
	glWidget->update();
}

void MainWindow::onSimplifyByAll() {
	AllOptionDialog dlg;
	if (dlg.exec()) {
		glWidget->simplifyByAll(dlg.getAlpha());
		glWidget->update();
	}
}

void MainWindow::onSimplifyByDP() {
	DPOptionDialog dlg;
	if (dlg.exec()) {
		glWidget->simplifyByDP(dlg.getEpsilon(), dlg.getLayeringThreshold(), dlg.getSnappingThreshold(), dlg.getOrientation() / 180.0 * CV_PI, dlg.getMinContourArea(), dlg.getMaxOBBRatio(), dlg.isAllowTriangleContour(), dlg.isAllowOverhang());
		glWidget->update();
	}
}

void MainWindow::onSimplifyByRightAngle() {
	RightAngleOptionDialog dlg;
	if (dlg.exec()) {
		glWidget->simplifyByRightAngle(dlg.getResolution(), dlg.getOptimization(), dlg.getLayeringThreshold(), dlg.getSnappingThreshold(), dlg.getOrientation() / 180.0 * CV_PI, dlg.getMinContourArea(), dlg.getMaxOBBRatio(), dlg.isAllowTriangleContour(), dlg.isAllowOverhang());
		glWidget->update();
	}
}

void MainWindow::onSimplifyByCurve() {
	CurveOptionDialog dlg;
	if (dlg.exec()) {
		glWidget->simplifyByCurve(dlg.getEpsilon(), dlg.getCurveThreshold(), dlg.getLayeringThreshold(), dlg.getSnappingThreshold(), dlg.getOrientation() / 180.0 * CV_PI, dlg.getMinContourArea(), dlg.getMaxOBBRatio(), dlg.isAllowTriangleContour(), dlg.isAllowOverhang());
		glWidget->update();
	}
}

void MainWindow::onSimplifyByCurveRightAngle() {
	CurveRightAngleOptionDialog dlg;
	if (dlg.exec()) {
		glWidget->simplifyByCurveRightAngle(dlg.getEpsilon(), dlg.getCurveThreshold(), dlg.getAngleThreshold() / 180.0 * CV_PI, dlg.getLayeringThreshold(), dlg.getSnappingThreshold(), dlg.getOrientation() / 180.0 * CV_PI, dlg.getMinContourArea(), dlg.getMaxOBBRatio(), dlg.isAllowTriangleContour(), dlg.isAllowOverhang());
		glWidget->update();
	}
}

void MainWindow::onOffsetScale() {
	OffsetScaleDialog dlg;
	dlg.setOffset(glWidget->offset.x, glWidget->offset.y, glWidget->offset.z);
	dlg.setScale(glWidget->scale);
	if (dlg.exec()) {
		glWidget->offset = glm::dvec3(dlg.getOffsetX(), dlg.getOffsetY(), dlg.getOffsetZ());
		glWidget->scale = dlg.getScale();
		glWidget->update3DGeometry();
		glWidget->update();
	}
}

void MainWindow::onColoringModeChanged() {
	if (ui.actionColor->isChecked()) {
		glWidget->color_mode = GLWidget3D::COLOR;
	}
	else if (ui.actionTexture->isChecked()) {
		glWidget->color_mode = GLWidget3D::TEXTURE;
	}
	glWidget->update3DGeometry();
	glWidget->update();
}

void MainWindow::onRenderingModeChanged() {
	if (ui.actionRenderingBasic->isChecked()) {
		glWidget->renderManager.renderingMode = RenderManager::RENDERING_MODE_BASIC;
	}
	else if (ui.actionRenderingSSAO->isChecked()) {
		glWidget->renderManager.renderingMode = RenderManager::RENDERING_MODE_SSAO;
	}
	else if (ui.actionRenderingHatching->isChecked()) {
		glWidget->renderManager.renderingMode = RenderManager::RENDERING_MODE_HATCHING;
	}
	glWidget->update();
}