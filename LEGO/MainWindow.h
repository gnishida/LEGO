#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"
#include "GLWidget3D.h"

class MainWindow : public QMainWindow {
	Q_OBJECT

private:
	Ui::MainWindowClass ui;
	GLWidget3D* glWidget;

public:
	MainWindow(QWidget *parent = 0);
	~MainWindow();


public slots:
	void onOpen();
	void onSaveOBJ();
	void onSaveTopFaces();
	void onSavePLY();
	void onSaveImage();
	void onInputVoxel();
	void onSimplifyByAll();
	void onSimplifyByDP();
	void onSimplifyByRightAngle();
	void onSimplifyByCurve();
	void onSimplifyByCurveRightAngle();
	void onOffsetScale();
	void onColoringModeChanged();
	void onRenderingModeChanged();
};

#endif // MAINWINDOW_H
