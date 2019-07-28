#include "software_renderer.h"

#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>

#include "triangulation.h"

using namespace std;

namespace CS248 {



	int nearest_pixel(float v) {
		return static_cast<int>(floor(v));
	}

	// Implements SoftwareRenderer //

	// fill a sample location with color
	void SoftwareRendererImp::fill_sample(int sx, int sy, const Color& color) {
		this->sample_buffer.set(sx, sy, color);
	}

	// fill samples in the entire pixel specified by pixel coordinates
	void SoftwareRendererImp::fill_pixel(int x, int y, const Color& color) {

		// TODO: Colors come in the range of [0,1]!

		// Task 2: Re-implement this function

		// check bounds
		if (!is_valid_target_pixel(x, y)) return;

		this->sample_buffer.set_pixel(x, y, color);

		/*
		Color pixel_color;
		float inv255 = 1.0 / 255.0;
		pixel_color.r = render_target[4 * (x + y * target_w)] * inv255;
		pixel_color.g = render_target[4 * (x + y * target_w) + 1] * inv255;
		pixel_color.b = render_target[4 * (x + y * target_w) + 2] * inv255;
		pixel_color.a = render_target[4 * (x + y * target_w) + 3] * inv255;

		pixel_color = ref->alpha_blending_helper(pixel_color, color);

		render_target[4 * (x + y * target_w)] = (uint8_t)(pixel_color.r * 255);
		render_target[4 * (x + y * target_w) + 1] = (uint8_t)(pixel_color.g * 255);
		render_target[4 * (x + y * target_w) + 2] = (uint8_t)(pixel_color.b * 255);
		render_target[4 * (x + y * target_w) + 3] = (uint8_t)(pixel_color.a * 255);
		*/
	}

	void SoftwareRendererImp::draw_svg(SVG& svg) {

		// set top level transformation
		transformation = canvas_to_screen;

		// clear sample buffer
		this->sample_buffer.clear(Color(255, 255, 255, 255));

		// draw all elements
		for (size_t i = 0; i < svg.elements.size(); ++i) {
			draw_element(svg.elements[i]);
		}

		// draw canvas outline
		Vector2D a = transform(Vector2D(0, 0)); a.x--; a.y--;
		Vector2D b = transform(Vector2D(svg.width, 0)); b.x++; b.y--;
		Vector2D c = transform(Vector2D(0, svg.height)); c.x--; c.y++;
		Vector2D d = transform(Vector2D(svg.width, svg.height)); d.x++; d.y++;

		rasterize_line(a.x, a.y, b.x, b.y, Color::Black);
		rasterize_line(a.x, a.y, c.x, c.y, Color::Black);
		rasterize_line(d.x, d.y, b.x, b.y, Color::Black);
		rasterize_line(d.x, d.y, c.x, c.y, Color::Black);

		// resolve and send to render target
		resolve();
	}

	void SoftwareRendererImp::set_sample_rate(size_t sample_rate) {
		this->sample_rate = sample_rate;
		
		// TODO: Because of code bs, have to check that they are set
		if (target_h != 0 && target_w != 0) {
			this->sample_buffer = SampleBuffer(ref, target_h, target_w, sample_rate);
		}
	}

	void SoftwareRendererImp::set_render_target(unsigned char* render_target,
		size_t width, size_t height) {

		// Task 2: 
		// You may want to modify this for supersampling support
		this->render_target = render_target;
		this->target_w = width;
		this->target_h = height;
		
		// TODO: Because of code bs, have to check it is set
		if (sample_rate != 0) {
			this->sample_buffer = SampleBuffer(ref, target_h, target_w, sample_rate);
		}
	}

	void SoftwareRendererImp::draw_element(SVGElement* element) {

		// Task 3 (part 1):
		// Modify this to implement the transformation stack

		switch (element->type) {
		case POINT:
			draw_point(static_cast<Point&>(*element));
			break;
		case LINE:
			draw_line(static_cast<Line&>(*element));
			break;
		case POLYLINE:
			draw_polyline(static_cast<Polyline&>(*element));
			break;
		case RECT:
			draw_rect(static_cast<Rect&>(*element));
			break;
		case POLYGON:
			draw_polygon(static_cast<Polygon&>(*element));
			break;
		case ELLIPSE:
			draw_ellipse(static_cast<Ellipse&>(*element));
			break;
		case IMAGE:
			draw_image(static_cast<Image&>(*element));
			break;
		case GROUP:
			draw_group(static_cast<Group&>(*element));
			break;
		default:
			break;
		}

	}


	// Primitive Drawing //

	void SoftwareRendererImp::draw_point(Point& point) {
		Vector2D p = transform(point.position);
		rasterize_point(p.x, p.y, point.style.fillColor);
	}

	void SoftwareRendererImp::draw_line(Line& line) {
		Vector2D p0 = transform(line.from);
		Vector2D p1 = transform(line.to);
		rasterize_line(p0.x, p0.y, p1.x, p1.y, line.style.strokeColor);
	}

	void SoftwareRendererImp::draw_polyline(Polyline& polyline) {

		Color c = polyline.style.strokeColor;

		if (c.a != 0) {
			int nPoints = polyline.points.size();
			for (int i = 0; i < nPoints - 1; i++) {
				Vector2D p0 = transform(polyline.points[(i + 0) % nPoints]);
				Vector2D p1 = transform(polyline.points[(i + 1) % nPoints]);
				rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
			}
		}
	}

	void SoftwareRendererImp::draw_rect(Rect& rect) {

		Color c;

		// draw as two triangles
		float x = rect.position.x;
		float y = rect.position.y;
		float w = rect.dimension.x;
		float h = rect.dimension.y;

		Vector2D p0 = transform(Vector2D(x, y));
		Vector2D p1 = transform(Vector2D(x + w, y));
		Vector2D p2 = transform(Vector2D(x, y + h));
		Vector2D p3 = transform(Vector2D(x + w, y + h));

		// draw fill
		c = rect.style.fillColor;
		if (c.a != 0) {
			rasterize_triangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, c);
			rasterize_triangle(p2.x, p2.y, p1.x, p1.y, p3.x, p3.y, c);
		}

		// draw outline
		c = rect.style.strokeColor;
		if (c.a != 0) {
			rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
			rasterize_line(p1.x, p1.y, p3.x, p3.y, c);
			rasterize_line(p3.x, p3.y, p2.x, p2.y, c);
			rasterize_line(p2.x, p2.y, p0.x, p0.y, c);
		}

	}

	void SoftwareRendererImp::draw_polygon(Polygon& polygon) {

		Color c;

		// draw fill
		c = polygon.style.fillColor;
		if (c.a != 0) {

			// triangulate
			vector<Vector2D> triangles;
			triangulate(polygon, triangles);

			// draw as triangles
			for (size_t i = 0; i < triangles.size(); i += 3) {
				Vector2D p0 = transform(triangles[i + 0]);
				Vector2D p1 = transform(triangles[i + 1]);
				Vector2D p2 = transform(triangles[i + 2]);
				rasterize_triangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, c);
			}
		}

		// draw outline
		c = polygon.style.strokeColor;
		if (c.a != 0) {
			int nPoints = polygon.points.size();
			for (int i = 0; i < nPoints; i++) {
				Vector2D p0 = transform(polygon.points[(i + 0) % nPoints]);
				Vector2D p1 = transform(polygon.points[(i + 1) % nPoints]);
				rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
			}
		}
	}

	void SoftwareRendererImp::draw_ellipse(Ellipse& ellipse) {

		// Extra credit 

	}

	void SoftwareRendererImp::draw_image(Image& image) {

		Vector2D p0 = transform(image.position);
		Vector2D p1 = transform(image.position + image.dimension);

		rasterize_image(p0.x, p0.y, p1.x, p1.y, image.tex);
	}

	void SoftwareRendererImp::draw_group(Group& group) {

		for (size_t i = 0; i < group.elements.size(); ++i) {
			draw_element(group.elements[i]);
		}

	}

	// Rasterization //

	bool SoftwareRendererImp::is_valid_target_pixel(int x, int y) const {
		return !((x < 0 || x >= target_w) || (y < 0 || y >= target_h));
	}

	// The input arguments in the rasterization functions 
	// below are all defined in screen space coordinates

	void SoftwareRendererImp::rasterize_point(float x, float y, Color color) {

		// fill in the nearest pixel
		int sx = nearest_pixel(x);
		int sy = nearest_pixel(y);

		// check bounds
		if (!is_valid_target_pixel(sx, sy)) return;

		// TODO: fill_sample vs fill_pixel? Depends if you want points very visible or not.
		fill_pixel(sx, sy, color);

		/*
		// fill sample - NOT doing alpha blending!
		// TODO: Call fill_pixel here to run alpha blending
		render_target[4 * (sx + sy * target_w)] = (uint8_t)(color.r * 255);
		render_target[4 * (sx + sy * target_w) + 1] = (uint8_t)(color.g * 255);
		render_target[4 * (sx + sy * target_w) + 2] = (uint8_t)(color.b * 255);
		render_target[4 * (sx + sy * target_w) + 3] = (uint8_t)(color.a * 255);
		*/
	}

	void SoftwareRendererImp::rasterize_line(float x0, float y0,
		float x1, float y1,
		Color color) {

		// Extra credit (delete the line below and implement your own)
		ref->rasterize_line_helper(x0, y0, x1, y1, target_w, target_h, color, this);

	}

	Vector2D create_tangent(const Primitive::Point& p0, const Primitive::Point& p1) {
		return Vector2D(static_cast<double>(p1.x) - p0.x, static_cast<double>(p1.y) - p0.y);
	}

	Vector2D create_normal(const Primitive::Point& p0, const Primitive::Point& p1) {
		return Vector2D(-1 * (static_cast<double>(p1.y) - p0.y), static_cast<double>(p1.x) - p0.x);
	}

	/**
	 * \brief Test if point is inside or outside a line formed by \a p0 and p1.
	 *
	 * \return >0 if inside, <0 if outside, 0 if on the line.
	 */
	float line_test(Primitive::Point p0, Primitive::Point p1, Primitive::Point px) {
		// TODO: use barycentric coords
		return -(px.x - p0.x) * (p1.y - p0.y) + (px.y - p0.y) * (p1.x - p0.x);
	}

	/**
	 * \param p0, p1, p2 Points that define the triangle.
	 */
	bool inside_triangle(const Primitive::Point& p0, const Primitive::Point& p1, const Primitive::Point& p2, Primitive::Point px) {
		// TODO: OGL Inside rules:
		// If on line, only inside if it meets one of these criteria:
		// - top edge: horizontal edge above all other edges
		// - left edge: non-horizontal edge that is to the left of other edges. May have more than 1

		const float l0 = line_test(p0, p1, px);
		const float l1 = line_test(p1, p2, px);
		const float l2 = line_test(p2, p0, px);

		return l0 >= 0 && l1 >= 0 && l2 >= 0;
	}

	float SoftwareRendererImp::get_sample_viewport_offset() const {
		return 1.0f / this->sample_rate;
	}

	void SoftwareRendererImp::rasterize_triangle(float x0, float y0,
		float x1, float y1,
		float x2, float y2,
		Color color) 
	{
		// Task 1: 
		// Implement triangle rasterization (you may want to call fill_sample here)

		// Check triangle bounds
		int x_max = ceil(max(x0, max(x1, x2)));
		int y_max = ceil(max(y0, max(y1, y2)));
		int x_min = floor(min(x0, min(x1, x2)));
		int y_min = floor(min(y0, min(y1, y2)));

		// Adjust for target bounds
		x_max = min(x_max, static_cast<int>(target_w));
		y_max = min(y_max, static_cast<int>(target_h));
		x_min = max(x_min, 0);
		y_min = max(y_min, 0);

		/*
		 * Track the viewport position for bounds check and the corresponding 
		 * sample position for populating the samples.
		 */
		const float sample_offset = get_sample_viewport_offset();
		const float initial_sample_offset = sample_offset / sample_rate;
	
		float y = y_min + sample_offset;
		size_t sy = y_min * sample_rate;

		const float x_start = x_min + sample_offset;
		const size_t sx_start = x_min * sample_rate;
		
		float x;
		size_t sx;
		while (y < y_max) {
			x = x_start;
			sx = x_min * sample_rate;
			while (x < x_max) {

				if (inside_triangle({ x0, y0 }, { x1, y1 }, { x2, y2 }, { x, y })) {
					fill_sample(sx, sy, color);
				}

				x += sample_offset;
				++sx;
			}

			y += sample_offset;
			++sy;
		}
	}

	void SoftwareRendererImp::rasterize_image(float x0, float y0,
		float x1, float y1,
		Texture& tex) {
		// Task 4: 
		// Implement image rasterization (you may want to call fill_sample here)

	}

	void SoftwareRendererImp::set_render_target_pixel(size_t x, size_t y, const Color& color) {
		render_target[4 * (x + y * target_w) + 0] = static_cast<uint8_t>(color.r * 255);
		render_target[4 * (x + y * target_w) + 1] = static_cast<uint8_t>(color.g * 255);
		render_target[4 * (x + y * target_w) + 2] = static_cast<uint8_t>(color.b * 255);
		render_target[4 * (x + y * target_w) + 3] = static_cast<uint8_t>(color.a * 255);
	}

	// resolve samples to render target
	void SoftwareRendererImp::resolve(void) {

		// Task 2: 
		// Implement supersampling
		// You may also need to modify other functions marked with "Task 2".

		for (size_t y = 0; y < target_h; ++y) {
			for (size_t x = 0; x < target_w; ++x) {
				set_render_target_pixel(x, y, this->sample_buffer.resolve_pixel(x, y));
			}
		}

		return;

	}



	/**
	* Resolves the value for a pixel.
	*/


	/**
	* \brief Sets values for a full pixel.
	*/


	/**
	* \param sample_rate Number of samples per axis. Ie. 2 => 4 samples, 4 => 16 samples.
	*/

	inline SampleBuffer::SampleBuffer(SoftwareRendererRef* ref, size_t w, size_t h, size_t sample_rate)
		: FrameBuffer(ref, h* sample_rate, w* sample_rate), sample_rate(sample_rate) {}

	inline void SampleBuffer::clear(const Color& color) {
		std::fill(buf.begin(), buf.end(), color);
	}

	inline void SampleBuffer::set_sample_rate(size_t sample_rate) {
		this->sample_rate = sample_rate;
	}

	inline void SampleBuffer::set_target_dim(size_t w, size_t h) {
		this->h = h;
		this->w = w;
	}

	inline void SampleBuffer::set_pixel(size_t pixel_x, size_t pixel_y, const Color& color) {
		for (size_t y = pixel_y * sample_rate; y < (pixel_y + 1) * sample_rate; ++y) {
			for (size_t x = pixel_x * sample_rate; x < (pixel_x + 1) * sample_rate; ++x) {
				set(x, y, color);
			}
		}
	}

	inline Color SampleBuffer::resolve_pixel(size_t pixel_x, size_t pixel_y) {
		Color col;

		for (size_t y = pixel_y * sample_rate; y < (pixel_y + 1) * sample_rate; ++y) {
			for (size_t x = pixel_x * sample_rate; x < (pixel_x + 1) * sample_rate; ++x) {
				col += get(x, y);
			}
		}

		col /= sample_rate * sample_rate;
		if (col.r > 1.0f) { col.r = 1.0f; }
		if (col.b > 1.0f) { col.b = 1.0f; }
		if (col.g > 1.0f) { col.g = 1.0f; }
		if (col.a > 1.0f) { col.a = 1.0f; }

		return col;
	}

	inline FrameBuffer::FrameBuffer(SoftwareRendererRef* ref, size_t w, size_t h) : ref(ref), h(h), w(w) {
		buf.resize(h * w, Color(1, 1, 1, 1)); // TODO: color is expected to be 255 in buffer, but [0,1] when in color
	}

	inline void FrameBuffer::set(size_t x, size_t y, const Color& color) {
		constexpr float inv255 = 1.0f / 255.0f;

		Color pixel_col = get(x, y);// * inv255;
		pixel_col = ref->alpha_blending_helper(pixel_col, color);
		buf[buf_pos(x, y)] = color;
	}

	inline Color FrameBuffer::get(size_t x, size_t y) const {
		return buf[buf_pos(x, y)];
	}

	inline size_t FrameBuffer::buf_pos(size_t x, size_t y) const {
		const size_t i = y * w + x;
		assert(i < buf.size());
		return i;
	}

	inline size_t FrameBuffer::buf_pos(const Primitive::Point& pixel) const {
		const size_t i = static_cast<size_t>(nearest_pixel(pixel.y)) * w + static_cast<size_t>(nearest_pixel(pixel.x));
		assert(i < buf.size());
		return i;
	}

} // namespace CS248
