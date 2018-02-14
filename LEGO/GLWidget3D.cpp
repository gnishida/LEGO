#include "GLWidget3D.h"
#include "MainWindow.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <QDir>
#include <QTextStream>
#include <QDate>
#include <iostream>
#include <QProcess>
#include "GLUtils.h"
#include "ContourUtils.h"
#include "BuildingSimplification.h"
#include "DisjointVoxelData.h"
#include "PlyWriter.h"

GLWidget3D::GLWidget3D(MainWindow *parent) : QGLWidget(QGLFormat(QGL::SampleBuffers)) {
	this->mainWin = parent;
	ctrlPressed = false;
	shiftPressed = false;

	first_paint = true;

	// This is necessary to prevent the screen overdrawn by OpenGL
	setAutoFillBackground(false);

	// light direction for shadow mapping
	light_dir = glm::normalize(glm::vec3(-1, 1, -3));

	// model/view/projection matrices for shadow mapping
	glm::mat4 light_pMatrix = glm::ortho<float>(-300, 300, -300, 300, 0.1, 600);
	glm::mat4 light_mvMatrix = glm::lookAt(-light_dir * 50.0f, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	light_mvpMatrix = light_pMatrix * light_mvMatrix;

	// spot light
	spot_light_pos = glm::vec3(2, 2.5, 3);

	color_mode = COLOR_SAME;
	show_mode = SHOW_INPUT;
}

/**
* Draw the scene.
*/
void GLWidget3D::drawScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(true);

	renderManager.renderAll();
}

void GLWidget3D::render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PASS 1: Render to texture
	glUseProgram(renderManager.programs["pass1"]);

	glBindFramebuffer(GL_FRAMEBUFFER, renderManager.fragDataFB);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderManager.fragDataTex[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, renderManager.fragDataTex[1], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, renderManager.fragDataTex[2], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, renderManager.fragDataTex[3], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderManager.fragDepthTex, 0);

	// Set the list of draw buffers.
	GLenum DrawBuffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glDrawBuffers(4, DrawBuffers); // "3" is the size of DrawBuffers
	// Always check that our framebuffer is ok
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("+ERROR: GL_FRAMEBUFFER_COMPLETE false\n");
		exit(0);
	}

	glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["pass1"], "mvpMatrix"), 1, false, &camera.mvpMatrix[0][0]);
	glUniform3f(glGetUniformLocation(renderManager.programs["pass1"], "lightDir"), light_dir.x, light_dir.y, light_dir.z);
	glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["pass1"], "light_mvpMatrix"), 1, false, &light_mvpMatrix[0][0]);
	glUniform3f(glGetUniformLocation(renderManager.programs["pass1"], "spotLightPos"), spot_light_pos.x, spot_light_pos.y, spot_light_pos.z);
	glUniform3f(glGetUniformLocation(renderManager.programs["pass1"], "cameraPos"), camera.pos.x, camera.pos.y, camera.pos.z);

	glUniform1i(glGetUniformLocation(renderManager.programs["pass1"], "shadowMap"), 6);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, renderManager.shadow.textureDepth);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	drawScene();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PASS 2: Create AO
	if (renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
		glUseProgram(renderManager.programs["ssao"]);
		glBindFramebuffer(GL_FRAMEBUFFER, renderManager.fragDataFB_AO);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderManager.fragAOTex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderManager.fragDepthTex_AO, 0);
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Always check that our framebuffer is ok
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			printf("++ERROR: GL_FRAMEBUFFER_COMPLETE false\n");
			exit(0);
		}

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["ssao"], "pixelSize"), 2.0f / this->width(), 2.0f / this->height());

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex0"), 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex1"), 2);
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "noiseTex"), 7);
		glActiveTexture(GL_TEXTURE7);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragNoiseTex);

		{
			glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["ssao"], "mvpMatrix"), 1, false, &camera.mvpMatrix[0][0]);
			glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["ssao"], "pMatrix"), 1, false, &camera.pMatrix[0][0]);
		}

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "uKernelSize"), renderManager.uKernelSize);
		glUniform3fv(glGetUniformLocation(renderManager.programs["ssao"], "uKernelOffsets"), renderManager.uKernelOffsets.size(), (const GLfloat*)renderManager.uKernelOffsets.data());

		glUniform1f(glGetUniformLocation(renderManager.programs["ssao"], "uPower"), renderManager.uPower);
		glUniform1f(glGetUniformLocation(renderManager.programs["ssao"], "uRadius"), renderManager.uRadius);

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}
	else if (renderManager.renderingMode == RenderManager::RENDERING_MODE_LINE || renderManager.renderingMode == RenderManager::RENDERING_MODE_HATCHING) {
		glUseProgram(renderManager.programs["line"]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["line"], "pixelSize"), 1.0f / this->width(), 1.0f / this->height());
		glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["line"], "pMatrix"), 1, false, &camera.pMatrix[0][0]);
		if (renderManager.renderingMode == RenderManager::RENDERING_MODE_HATCHING) {
			glUniform1i(glGetUniformLocation(renderManager.programs["line"], "useHatching"), 1);
		}
		else {
			glUniform1i(glGetUniformLocation(renderManager.programs["line"], "useHatching"), 0);
		}

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex0"), 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex1"), 2);
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex3"), 4);
		glActiveTexture(GL_TEXTURE4);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[3]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "hatchingTexture"), 5);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_3D, renderManager.hatchingTextures);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}
	else if (renderManager.renderingMode == RenderManager::RENDERING_MODE_CONTOUR) {
		glUseProgram(renderManager.programs["contour"]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["contour"], "pixelSize"), 1.0f / this->width(), 1.0f / this->height());

		glUniform1i(glGetUniformLocation(renderManager.programs["contour"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Blur

	if (renderManager.renderingMode == RenderManager::RENDERING_MODE_BASIC || renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUseProgram(renderManager.programs["blur"]);
		glUniform2f(glGetUniformLocation(renderManager.programs["blur"], "pixelSize"), 2.0f / this->width(), 2.0f / this->height());
		//printf("pixelSize loc %d\n", glGetUniformLocation(vboRenderManager.programs["blur"], "pixelSize"));

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex0"), 1);//COLOR
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex1"), 2);//NORMAL
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		/*glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);*/

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex3"), 4);//AO
		glActiveTexture(GL_TEXTURE4);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragAOTex);

		if (renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
			glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "ssao_used"), 1); // ssao used
		}
		else {
			glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "ssao_used"), 0); // no ssao
		}

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);

	}

	// REMOVE
	glActiveTexture(GL_TEXTURE0);
}

void GLWidget3D::loadVoxelData(const QString& filename) {
	// get directory
	QDir dir = QFileInfo(filename).absoluteDir();

	// scan all the files in the directory to get a voxel data
	QStringList files = dir.entryList(QDir::NoDotAndDotDot | QDir::Files, QDir::DirsFirst);
	std::vector<cv::Mat_<uchar>> voxel_data(files.size());
	for (int i = 0; i < files.size(); i++) {
		voxel_data[i] = cv::imread((dir.absolutePath() + "/" + files[i]).toUtf8().constData(), cv::IMREAD_GRAYSCALE);
	}

	// count the voxel
	voxel_count = 0;
	for (int i = 0; i < voxel_data.size(); i++) {
		voxel_count += util::calculateArea(voxel_data[i]);
	}

	// find the ground level
	double max_area = 0;
	for (int i = 0; i < voxel_data.size(); i++) {
		max_area = std::max(max_area, util::calculateArea(voxel_data[i]));
	}
	for (ground_level = 0; ground_level < voxel_data.size(); ground_level++) {
		if (util::calculateArea(voxel_data[ground_level]) > max_area * 0.5) break;
	}

	disjoint_voxel_data.disjoint(voxel_data, 0.5);
	
	//disjointed_voxel_data.resize(disjoint_voxel_data.size());
	layers.clear();
	for (int i = 0; i < disjoint_voxel_data.size(); i++) {
		std::vector<cv::Mat_<uchar>> voxel_data = disjoint_voxel_data.getDisjointedVoxelData(i);

		try {
			util::LayerVoxelData lvd(voxel_data, 0.5);
			std::shared_ptr<util::Layer> layer = lvd.layering(0.8);
			layers.push_back(layer);
		}
		catch (...) {}
	}
	
	show_mode = SHOW_INPUT;
	update3DGeometry();
}

void GLWidget3D::savePLY(const QString& filename) {
	util::ply::PlyWriter::write(filename.toUtf8().constData(), buildings);
}

void GLWidget3D::saveImage(const QString& filename) {
	QImage image = grabFrameBuffer();
	image.save(filename);
}

void GLWidget3D::showInputVoxel() {
	show_mode = SHOW_INPUT;
	update3DGeometry(layers);
}

void GLWidget3D::simplifyByAll(double alpha) {
	buildings = simp::BuildingSimplification::simplifyBuildings(disjoint_voxel_data, simp::BuildingSimplification::ALG_ALL, alpha, 0.8, 0, 0, 0);

	show_mode = SHOW_ALL;
	update3DGeometry();
}

void GLWidget3D::simplifyByOpenCV(double epsilon, double layering_threshold, double snap_vertex_threshold, double snap_edge_threshold) {
	buildings = simp::BuildingSimplification::simplifyBuildings(disjoint_voxel_data, simp::BuildingSimplification::ALG_OPENCV, 0.5, layering_threshold, epsilon, 0, 0);

	show_mode = SHOW_OPENCV;
	update3DGeometry();
}

void GLWidget3D::simplifyByRightAngle(int resolution, double layering_threshold, double snap_vertex_threshold, double snap_edge_threshold) {
	buildings = simp::BuildingSimplification::simplifyBuildings(disjoint_voxel_data, simp::BuildingSimplification::ALG_RIGHTANGLE, 0.5, layering_threshold, 0, resolution, 0);

	show_mode = SHOW_RIGHTANGLE;
	update3DGeometry();
}

void GLWidget3D::simplifyByCurve(double epsilon, double curve_threshold, double layering_threshold, double snap_vertex_threshold, double snap_edge_threshold) {
	buildings = simp::BuildingSimplification::simplifyBuildings(disjoint_voxel_data, simp::BuildingSimplification::ALG_CURVE, 0.5, layering_threshold, epsilon, 0, curve_threshold);

	show_mode = SHOW_CURVE;
	update3DGeometry();
}

void GLWidget3D::opencvTest() {
	for (int alpha_idx = 0; alpha_idx <= 10; alpha_idx++) {
		double alpha = (double)alpha_idx * 0.1;

		QString filename = QString("opencv_alpha_%1.txt").arg(alpha);
		QFile file(filename);
		file.open(QIODevice::WriteOnly);
		QTextStream out(&file);

		// get baseline cost
		std::vector<std::shared_ptr<simp::Building>> baseline_buildings = simp::BuildingSimplification::simplifyBuildings(disjoint_voxel_data, simp::BuildingSimplification::ALG_OPENCV, alpha, 0.9, 0.5, 0, 0);
		std::vector<float> baseline_costs = simp::BuildingSimplification::sumCost(baseline_buildings);

		double min_cost = std::numeric_limits<double>::max();
		double best_layering_threshold;
		double best_epsilon;
		double best_error;
		double best_simplicity;
		double best_simplicity_baseline;

		const int num_layering_samples = 5;
		const int num_epsilon_samples = 5;
		for (int layering_idx = 0; layering_idx <= num_layering_samples; layering_idx++) {
			float layering_threshold = layering_idx * 1.0 / num_layering_samples + 0.0;
			for (int epsilon_idx = 0; epsilon_idx <= num_epsilon_samples; epsilon_idx++) {
				float epsilon = (float)epsilon_idx * 10 / num_epsilon_samples + 0.0;

				buildings = simp::BuildingSimplification::simplifyBuildings(disjoint_voxel_data, simp::BuildingSimplification::ALG_OPENCV, alpha, layering_threshold, epsilon, 0, 0);

				// calculate the cost
				std::vector<float> costs = simp::BuildingSimplification::sumCost(buildings);
				float cost = alpha * costs[0] / costs[1] + (1 - alpha) * costs[2] / baseline_costs[2];
			
				out << layering_threshold << "," << epsilon << "," << cost << endl;
				if (cost < min_cost) {
					min_cost = cost;
					best_layering_threshold = layering_threshold;
					best_epsilon = epsilon;
					best_error = costs[0] / costs[1];
					best_simplicity = costs[2];
					best_simplicity_baseline = baseline_costs[2];
				}
			}
		}
		file.close();

		std::cout << "----------------------------------------" << std::endl;
		std::cout << "alpha: " << alpha << std::endl;
		std::cout << "Best cost = " << min_cost << std::endl;
		std::cout << "Best layering threshold = " << best_layering_threshold << std::endl;
		std::cout << "Best epsilon = " << best_epsilon << std::endl;
		std::cout << "Best error = " << best_error * voxel_count << " / " << voxel_count << " (" << best_error << ")" << std::endl;
		std::cout << "Best simplicity = " << best_simplicity << " / " << best_simplicity_baseline << " (" << best_simplicity / best_simplicity_baseline << ")" << std::endl;
	}
}

void GLWidget3D::rightAngleTest() {
	for (int alpha_idx = 0; alpha_idx <= 10; alpha_idx++) {
		double alpha = (double)alpha_idx * 0.1;

		QString filename = QString("rightangle_alpha_%1.txt").arg(alpha);
		QFile file(filename);
		file.open(QIODevice::WriteOnly);
		QTextStream out(&file);

		// get baseline cost
		std::vector<std::shared_ptr<simp::Building>> baseline_buildings = simp::BuildingSimplification::simplifyBuildings(disjoint_voxel_data, simp::BuildingSimplification::ALG_OPENCV, alpha, 0.9, 0.5, 0, 0);
		std::vector<float> baseline_costs = simp::BuildingSimplification::sumCost(baseline_buildings);

		double min_cost = std::numeric_limits<double>::max();
		double best_layering_threshold;
		double best_resolution;
		double best_error;
		double best_simplicity;
		double best_simplicity_baseline;

		const int num_layering_samples = 5;
		const int num_resolution_samples = 5;
		for (int layering_idx = 0; layering_idx <= num_layering_samples; layering_idx++) {
			float layering_threshold = layering_idx * 0.8 / num_layering_samples + 0.1;
			for (int resolution_idx = 0; resolution_idx <= num_resolution_samples; resolution_idx++) {
				int resolution = (float)resolution_idx * 5 / num_resolution_samples + 2;

				std::cout << "layering_idx = " << layering_idx << ", " << "resolution_idx = " << resolution_idx << std::endl;

				buildings = simp::BuildingSimplification::simplifyBuildings(disjoint_voxel_data, simp::BuildingSimplification::ALG_RIGHTANGLE, alpha, layering_threshold, 0, resolution, 0);

				// calculate the cost
				std::vector<float> costs = simp::BuildingSimplification::sumCost(buildings);
				float cost = alpha * costs[0] / costs[1] + (1 - alpha) * costs[2] / baseline_costs[2];
				
				out << layering_threshold << "," << resolution << "," << cost << endl;
				if (cost < min_cost) {
					min_cost = cost;
					best_layering_threshold = layering_threshold;
					best_resolution = resolution;
					best_error = costs[0] / costs[1];
					best_simplicity = costs[2];
					best_simplicity_baseline = baseline_costs[2];
				}
			}
		}
		file.close();

		std::cout << "----------------------------------------" << std::endl;
		std::cout << "alpha: " << alpha << std::endl;
		std::cout << "Best cost = " << min_cost << std::endl;
		std::cout << "Best layering threshold = " << best_layering_threshold << std::endl;
		std::cout << "Best resolution = " << best_resolution << std::endl;
		std::cout << "Best error = " << best_error * voxel_count << " / " << voxel_count << " (" << best_error << ")" << std::endl;
		std::cout << "Best simplicity = " << best_simplicity << " / " << best_simplicity_baseline << " (" << best_simplicity / best_simplicity_baseline << ")" << std::endl;
	}
}

void GLWidget3D::update3DGeometry() {
	if (show_mode == SHOW_INPUT) {
		update3DGeometry(layers);
	}
	else {
		update3DGeometry(buildings);
	}
}

void GLWidget3D::update3DGeometry(const std::vector<std::shared_ptr<util::Layer>>& layers) {
	renderManager.removeObjects();

	if (layers.size() > 0 && layers[0]->slices.size() > 0) {
		cv::Size size(disjoint_voxel_data.voxel_data[0].cols, disjoint_voxel_data.voxel_data[0].rows);
		glm::vec4 color(0.7, 1, 0.7, 1);

		std::vector<Vertex> vertices;
		for (int i = 0; i < layers.size(); i++) {
			if (color_mode == COLOR_BY_BUILDING) {
				color = glm::vec4((float)(rand() % 80) / 100 + 0.2, (float)(rand() % 80) / 100 + 0.2, (float)(rand() % 80) / 100 + 0.2, 1);
			}
			update3DGeometry(layers[i], size, color, vertices);
		}
		renderManager.addObject("building", "", vertices, true);
	}

	std::vector<Vertex> vertices2;
	glutils::drawBox(300, 300, 1, glm::vec4(0.9, 1, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, 0)), vertices2);
	renderManager.addObject("ground", "", vertices2, true);

	// update shadow map
	renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
}

void GLWidget3D::update3DGeometry(std::shared_ptr<util::Layer> layer, const cv::Size& size, glm::vec4& color, std::vector<Vertex>& vertices) {
	if (color_mode == COLOR_BY_LAYER) {
		color = glm::vec4((float)(rand() % 80) / 100 + 0.2, (float)(rand() % 80) / 100 + 0.2, (float)(rand() % 80) / 100 + 0.2, 1);
	}

	for (int slice_id = 0; slice_id < layer->slices.size(); slice_id++) {
		for (int y = 0; y < layer->slices[slice_id].rows; y++) {
			for (int x = 0; x < layer->slices[slice_id].cols; x++) {
				if (layer->slices[slice_id](y, x) < 128) continue;
				glutils::drawBox(1, 1, 1, color, glm::translate(glm::mat4(), glm::vec3(x + 0.5 - size.width * 0.5, size.height * 0.5 - y - 0.5, layer->bottom_height + slice_id - ground_level + 0.5)), vertices);
			}
		}
	}

	for (int i = 0; i < layer->children.size(); i++) {
		update3DGeometry(layer->children[i], size, color, vertices);
	}
}

void GLWidget3D::update3DGeometry(const std::vector<std::shared_ptr<simp::Building>>& buildings) {
	renderManager.removeObjects();

	glm::vec4 color(0.7, 1, 0.7, 1);

	std::vector<Vertex> vertices;
	for (int i = 0; i < buildings.size(); i++) {
		if (color_mode == COLOR_BY_BUILDING) {
			color = glm::vec4((float)(rand() % 80) / 100 + 0.2, (float)(rand() % 80) / 100 + 0.2, (float)(rand() % 80) / 100 + 0.2, 1);
		}
		update3DGeometry(buildings[i], color, vertices);
	}
	renderManager.addObject("building", "", vertices, true);

	std::vector<Vertex> vertices2;
	glutils::drawBox(300, 300, 1, glm::vec4(0.9, 1, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, 0)), vertices2);
	renderManager.addObject("ground", "", vertices2, true);

	// update shadow map
	renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
}


void GLWidget3D::update3DGeometry(std::shared_ptr<simp::Building> building, glm::vec4& color, std::vector<Vertex>& vertices) {
	if (color_mode == COLOR_BY_LAYER) {
		color = glm::vec4((float)(rand() % 80) / 100 + 0.2, (float)(rand() % 80) / 100 + 0.2, (float)(rand() % 80) / 100 + 0.2, 1);
	}

	if (building->footprint.primitive_shapes.size() == 0) {
		std::vector<glm::dvec2> footprint(building->footprint.contour.size());
		for (int j = 0; j < building->footprint.contour.size(); j++) {
			cv::Point2f pt = building->footprint.contour.getActualPoint(j);
			footprint[j] = glm::dvec2(pt.x, pt.y);
		}
		std::vector<std::vector<glm::dvec2>> holes(building->footprint.holes.size());
		for (int j = 0; j < building->footprint.holes.size(); j++) {
			holes[j].resize(building->footprint.holes[j].size());
			for (int k = 0; k < building->footprint.holes[j].size(); k++) {
				cv::Point2f pt = building->footprint.holes[j].getActualPoint(k);
				holes[j][k] = glm::dvec2(pt.x, pt.y);
			}
		}

		if (building->footprint.holes.size() == 0) {
			glutils::drawPrism(footprint, building->top_height - building->bottom_height, color, glm::translate(glm::mat4(), glm::vec3(0, 0, building->bottom_height)), vertices);
		}
		else {
			glutils::drawPrismWithHoles(footprint, holes, building->top_height - building->bottom_height, color, glm::translate(glm::mat4(), glm::vec3(0, 0, building->bottom_height - ground_level)), vertices);
		}
	}
	else {
		for (int i = 0; i < building->footprint.primitive_shapes.size(); i++) {
			//
			std::vector<cv::Point2f> points = building->footprint.primitive_shapes[i]->getActualPoints();
			std::vector<glm::dvec2> pol(points.size());
			for (int j = 0; j < points.size(); j++) {
				pol[j] = glm::dvec2(points[j].x, points[j].y);
			}
			glutils::drawPrism(pol, building->top_height - building->bottom_height, color, glm::translate(glm::mat4(), glm::vec3(0, 0, building->bottom_height - ground_level)), vertices);
		}
	}

	for (int i = 0; i < building->children.size(); i++) {
		update3DGeometry(building->children[i], color, vertices);
	}
}

void GLWidget3D::keyPressEvent(QKeyEvent *e) {
	ctrlPressed = false;
	shiftPressed = false;

	if (e->modifiers() & Qt::ControlModifier) {
		ctrlPressed = true;
	}
	if (e->modifiers() & Qt::ShiftModifier) {
		shiftPressed = true;
	}

	switch (e->key()) {
	case Qt::Key_Space:
		break;
	default:
		break;
	}
}

void GLWidget3D::keyReleaseEvent(QKeyEvent* e) {
	switch (e->key()) {
	case Qt::Key_Control:
		ctrlPressed = false;
		break;
	case Qt::Key_Shift:
		shiftPressed = false;
		break;
	default:
		break;
	}
}

/**
* This function is called once before the first call to paintGL() or resizeGL().
*/
void GLWidget3D::initializeGL() {
	// init glew
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		std::cout << "Error: " << glewGetErrorString(err) << std::endl;
	}

	if (glewIsSupported("GL_VERSION_4_2"))
		printf("Ready for OpenGL 4.2\n");
	else {
		printf("OpenGL 4.2 not supported\n");
		exit(1);
	}
	const GLubyte* text = glGetString(GL_VERSION);
	printf("VERSION: %s\n", text);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable(GL_TEXTURE_2D);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexGenf(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenf(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glDisable(GL_TEXTURE_2D);

	glEnable(GL_TEXTURE_3D);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glDisable(GL_TEXTURE_3D);

	glEnable(GL_TEXTURE_2D_ARRAY);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glDisable(GL_TEXTURE_2D_ARRAY);

	////////////////////////////////
	renderManager.init("", "", "", true, 8192);
	renderManager.resize(this->width(), this->height());

	glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex0"), 0);//tex0: 0
}

/**
* This function is called whenever the widget has been resized.
*/
void GLWidget3D::resizeGL(int width, int height) {
	height = height ? height : 1;
	glViewport(0, 0, width, height);
	camera.updatePMatrix(width, height);

	renderManager.resize(width, height);
}

/**
* This function is called whenever the widget needs to be painted.
*/
void GLWidget3D::paintEvent(QPaintEvent *event) {
	if (first_paint) {
		std::vector<Vertex> vertices;
		glutils::drawQuad(0.001, 0.001, glm::vec4(1, 1, 1, 1), glm::mat4(), vertices);
		renderManager.addObject("dummy", "", vertices, true);
		renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
		first_paint = false;
	}

	// draw by OpenGL
	makeCurrent();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	render();

	// unbind texture
	glActiveTexture(GL_TEXTURE0);

	// restore the settings for OpenGL
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	QPainter painter(this);
	painter.setOpacity(1.0f);
	////
	painter.end();

	glEnable(GL_DEPTH_TEST);
}

/**
* This event handler is called when the mouse press events occur.
*/
void GLWidget3D::mousePressEvent(QMouseEvent *e) {
	// This is necessary to get key event occured even after the user selects a menu.
	setFocus();

	if (e->buttons() & Qt::LeftButton) {
		camera.mousePress(e->x(), e->y());
	}
	else if (e->buttons() & Qt::RightButton) {
	}
}

/**
* This event handler is called when the mouse move events occur.
*/

void GLWidget3D::mouseMoveEvent(QMouseEvent *e) {
	if (e->buttons() & Qt::LeftButton) {
		if (shiftPressed) {
			camera.move(e->x(), e->y());
		}
		else {
			camera.rotate(e->x(), e->y(), (ctrlPressed ? 0.1 : 1));
		}
	}

	update();
}

/**
* This event handler is called when the mouse release events occur.
*/
void GLWidget3D::mouseReleaseEvent(QMouseEvent *e) {
}

void GLWidget3D::wheelEvent(QWheelEvent* e) {
	camera.zoom(e->delta() * 0.2);
	update();
}
