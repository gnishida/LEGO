#pragma once
#pragma warning(disable:4996)

#include <vector>
#include <map>
#include <tuple>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Partition_traits_2.h>
#include <CGAL/partition_2.h>
#include <CGAL/point_generators_2.h>
#include <CGAL/random_polygon_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/create_offset_polygons_2.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/random_polygon_2.h>
#include <CGAL/Polygon_2.h>

namespace util {

	// The following definitions are for triangulation only.
	struct FaceInfo {
		FaceInfo() {}
		int nesting_level;
		bool in_domain(){
			return nesting_level % 2 == 1;
		}
	};

	typedef CGAL::Exact_predicates_inexact_constructions_kernel       K;
	typedef CGAL::Triangulation_vertex_base_2<K>                      Vb;
	typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo, K>    Fbb;
	typedef CGAL::Constrained_triangulation_face_base_2<K, Fbb>        Fb;
	typedef CGAL::Triangulation_data_structure_2<Vb, Fb>               TDS;
	typedef CGAL::Exact_predicates_tag                                Itag;
	typedef CGAL::Constrained_Delaunay_triangulation_2<K, TDS, Itag>  CDT;
	typedef CDT::Point                                                Point;
	typedef CGAL::Partition_traits_2<K>                         Traits;
	typedef Traits::Polygon_2                                   Polygon_2;
	typedef Traits::Point_2                                     Point_2;
	typedef Polygon_2::Vertex_iterator                          Vertex_iterator;
	typedef std::list<Polygon_2>                                Polygon_list;
	typedef CGAL::Creator_uniform_2<int, Point_2>               Creator;
	typedef CGAL::Random_points_in_square_2< Point_2, Creator > Point_generator;
	typedef boost::shared_ptr<Polygon_2>						PolygonPtr;

	class PrimitiveShape {
	public:
		cv::Mat_<float> mat;

	protected:
		PrimitiveShape() {}

	public:
		virtual boost::shared_ptr<PrimitiveShape> clone() = 0;
		virtual std::vector<cv::Point2f> getActualPoints() = 0;
	};

	class PrimitiveRectangle : public PrimitiveShape {
	public:
		cv::Point2f min_pt;
		cv::Point2f max_pt;

	public:
		PrimitiveRectangle(const cv::Mat_<float>& mat, const cv::Point2f& min_pt, const cv::Point2f& max_pt);
		boost::shared_ptr<PrimitiveShape> clone();
		std::vector<cv::Point2f> getActualPoints();
	};

	class PrimitiveTriangle : public PrimitiveShape {
	public:
		std::vector<cv::Point2f> points;

	public:
		PrimitiveTriangle(const cv::Mat_<float>& mat);
		PrimitiveTriangle(const cv::Mat_<float>& mat, const std::vector<cv::Point2f>& points);
		boost::shared_ptr<PrimitiveShape> clone();
		std::vector<cv::Point2f> getActualPoints();
	};

	class Ring {
	public:
		cv::Mat_<float> mat;
		std::vector<cv::Point2f> points;

	public:
		Ring();

		const cv::Point2f& front() const;
		cv::Point2f& front();
		const cv::Point2f& back() const;
		cv::Point2f& back();
		std::vector<cv::Point2f>::iterator begin();
		std::vector<cv::Point2f>::iterator end();
		size_t size() const;
		void clear();
		void resize(size_t s);
		const cv::Point2f& operator[](int index) const;
		cv::Point2f& operator[](int index);
		void push_back(const cv::Point2f& pt);
		void pop_back();
		void erase(std::vector<cv::Point2f>::iterator position);
		void translate(float x, float y);
		void transform(const cv::Mat_<float>& m);
		void clockwise();
		void counterClockwise();
		cv::Point2f getActualPoint(int index);
		Ring getActualPoints();
	};

	class Polygon {
	public:
		cv::Mat_<float> mat;
		Ring contour;
		std::vector<Ring> holes;

		// For the right angle simplification, we currently store the decomposed primitive shapes in this data.
		// When generating a 3D geometry, or producing PLY file, this data should be used.
		std::vector<boost::shared_ptr<PrimitiveShape>> primitive_shapes;

	public:
		Polygon() {}

		Polygon clone();
		void translate(float x, float y);
		void transform(const cv::Mat_<float>& m);
		void clockwise();
		void counterClockwise();
	};

	bool isClockwise(const std::vector<cv::Point2f>& polygon);
	void clockwise(std::vector<cv::Point2f>& polygon);
	void counterClockwise(std::vector<cv::Point2f>& polygon);
	
	std::vector<cv::Point> removeRedundantPoint(const std::vector<cv::Point>& polygon);
	std::vector<cv::Point2f> removeRedundantPoint(const std::vector<cv::Point2f>& polygon);

	cv::Rect boundingBox(const std::vector<cv::Point>& polygon);
	cv::Rect boundingBox(const std::vector<cv::Point2f>& polygon);
	bool withinPolygon(const cv::Point2f& pt, const Polygon& polygon);
	bool withinPolygon(const cv::Point2f& pt, const Ring& ring);
	double calculateIOU(const cv::Mat_<uchar>& img, const cv::Mat_<uchar>& img2);
	double calculateIOU(const cv::Mat_<uchar>& img1, const cv::Mat_<uchar>& img2, const cv::Rect& rect);
	double calculateArea(const cv::Mat_<uchar>& img);
	std::vector<Polygon> findContours(const cv::Mat_<uchar>& img);
	Ring addCornerToOpenCVContour(const std::vector<cv::Point>& polygon, const cv::Mat_<uchar>& img);
	void findContour(const cv::Mat_<uchar>& img, std::vector<cv::Point>& contour);
	void createImageFromContour(int width, int height, const std::vector<cv::Point>& contour, const cv::Point& offset, cv::Mat_<uchar>& result);
	void createImageFromPolygon(int width, int height, const Polygon& polygon, const cv::Point& offset, cv::Mat_<uchar>& result);

	void snapPolygon(const std::vector<cv::Point2f>& ref_polygon, std::vector<cv::Point2f>& polygon, float snap_vertex_threshold, float snap_edge_threshold);
	float dotProduct(const cv::Point2f& v1, const cv::Point2f& v2);
	float crossProduct(const cv::Point2f& v1, const cv::Point2f& v2);
	float closestPoint(const cv::Point2f& a, const cv::Point2f& b, const cv::Point2f& c, bool segmentOnly, cv::Point2f& pt);

	// tessellation
	std::vector<std::vector<cv::Point2f>> tessellate(const Ring& points);
	std::vector<std::vector<cv::Point2f>> tessellate(const Ring& points, const std::vector<Ring>& holes);
	void mark_domains(CDT& ct, CDT::Face_handle start, int index, std::list<CDT::Edge>& border);
	void mark_domains(CDT& cdt);

}
