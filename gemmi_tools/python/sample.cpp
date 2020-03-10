#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <gemmi/grid.hpp>
#include "gemmi/unitcell.hpp"


namespace py = pybind11;
using namespace gemmi;

/*
template<typename T>
struct Location
{
	T x, y, z;

	Location(T a, T b, T c)
	{
		x = a;
		y = b;
		z = c;
	}
};

template<typename T>
struct Scale
{
	T s;


	Scale(T sc)
	{
		s = sc;
	}

};

template<typename T>
struct Shape {
	T x, y, z;

	template<typename T>
	Shape(T a, T b, T c)
	{
		x = a;
		y = b;
		z = c;
	}
};

template<typename T>
struct Orientation {
	T x00, x01, x02, x10, x11, x12, x20, x21, x22;

	Orientation(T y00, T y01, T y02, T y10, T y11, T y12, T y20, T y21, T y22)
	{
		x00 = y00;
		x01 = y01;
		x02 = y02;

		x10 = y10;
		x11 = y11;
		x12 = y12;

		x20 = y20;
		x21 = y21;
		x22 = y22;
	}

};

template<typename T>
GridBase<T> get_grid(Orientation orientation, Location location, Scale scale, Shape shape)
{
	GridBase<T> grid;

	return grid;
}

*/

/*
template<typename T, typename Z>
std::map<T, Z> Grid<T> get_sample_values(Grid<T> base_grid, Grid<T> sample_grid)
{

	for (int w = 0; w <= sample_grid.dw; ++w)
		for (int v = 0; v <= sample_grid.dv; ++v)
			for (int u = 0; u <= sample_grid.du; ++u) {

				sample_point = sample_grid.get_point(u, v, w);

				sample_position = sample_grid.point_to_position(sample_point)

				sampled_value = base_grid.interpolate_value(const Position & ctr);

				sample_grid.set_value(x, y, z, sampled_value) interpolate_value(const Position & ctr);

			}

	return sample_grid;

}
*/
/*
template<typename T, typename Z>
std::map<T, Z> Grid<T> get_sample_values(Grid<T> base_grid, Grid<T> sample_grid)
{

	for (int w = 0; w <= sample_grid.dw; ++w)
		for (int v = 0; v <= sample_grid.dv; ++v)
			for (int u = 0; u <= sample_grid.du; ++u) {

				sample_point = sample_grid.get_point(u, v, w);

				sample_position = sample_grid.point_to_position(sample_point)

				sampled_value = base_grid.interpolate_value(const Position & ctr);

				sample_grid.set_value(x, y, z, sampled_value) interpolate_value(const Position & ctr);

			}

	return sample_grid;

}
*/


template<typename T>
std::map<std::vector<int>, gemmi::Position> 
get_sample_positions(py::array_t<T> sample_points, py::array_t<T> sample_positions)
{
	auto pt = sample_points.mutable_unchecked<2>();
	auto ps = sample_positions.mutable_unchecked<2>();
	
	std::map<std::vector<int>, gemmi::Position> points;

	for (ssize_t i = 0; i < pt.shape(0); i++)
	{
		
		std::vector<int> location = { pt(i,0), pt(i,0), pt(i,1), pt(i,2)};
		gemmi::Position position(ps(i, 0), ps(i, 1), ps(i, 2));

		points.insert(std::pair(location, point));
	}

	return points;

}

template<typename T>
void fill_array(py::array_t<T> sample_array, 
	std::map<std::vector<int>, gemmi::Position> sample_points)
{

	auto r = sample_array.mutable_unchecked<3>(); // Will throw if ndim != 2 or flags.writeable is false
	for (ssize_t i = 0; i < r.shape(0); i++)
	{
		for (ssize_t j = 0; j < r.shape(1); j++)
		{
			for (ssize_t k = 0; k < r.shape(2); k++)
			{
				std::vector<int> point = { i, j, k };
				gemmi::Position position = sample_points[point];
				r(i, j, k) = base_grid.interpolate_value(position);
			}
		}
	}

	return sample_array;
}




void add_sample(py::module& m) {

	m.def("sample",
		[](py::array_t<float> sample_array, 
			py::array_t<float> sample_points,
			py::array_t<float> sample_positions, 
			gemmi::Grid<float> grid) 
		{
			
			std::map<std::vector<int>, gemmi::Position> sample_positions_map = get_sample_positions(sample_points, sample_positions);

			// std::map<std::vector<int>, T> sample_values = sample(grid, sample_positions);

			py::array_t<T> arr = fill_array(sample_array, sample_positions_map);
		
			return arr;
		},
		)

}

PYBIND11_MODULE(gemmi, mg) {
	mg.doc() = "General MacroMolecular I/O";
	mg.attr("__version__") = GEMMI_VERSION;
	add_sample(mg);
	
}

/*
auto r = x.mutable_unchecked<3>();
orientation_obj = Oreintation(r(0, 0), r(0, 1), r(0, 2),
	r(1, 0), r(1, 1), r(1, 2),
	r(2, 0), r(2, 1), r(2, 2));

translation_obj = ;

new_grid = ;
*/