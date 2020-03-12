#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include <gemmi/grid.hpp>
#include <gemmi/unitcell.hpp>

#include <gemmi_tools/sample.hpp>

namespace py = pybind11;
using namespace gemmi;


template<typename T>
std::map<std::vector<int>, gemmi::Position> 
get_sample_positions(py::array_t<int> sample_points, py::array_t<T> sample_positions)
{
	auto pt = sample_points.mutable_unchecked();
	auto ps = sample_positions.mutable_unchecked();
	
	std::map<std::vector<int>, gemmi::Position> points;

	for (ssize_t i = 0; i < pt.shape(0); i++)
	{
		
		std::vector<int> location = { pt(i,0), pt(i,0), pt(i,1), pt(i,2)};
		gemmi::Position position(ps(i, 0), ps(i, 1), ps(i, 2));

		points.insert(std::pair<std::vector<int>, gemmi::Position>(location, position));
	}

	return points;

}

template<typename T>
void fill_array(py::array_t<T> sample_array, std::map<std::vector<int>, gemmi::Position> sample_points, gemmi::Grid<T> grid)
{

	auto r = sample_array.mutable_unchecked(); // Will throw if ndim != 2 or flags.writeable is false
	for (ssize_t i = 0; i < r.shape(0); i++)
	{
		for (ssize_t j = 0; j < r.shape(1); j++)
		{
			for (ssize_t k = 0; k < r.shape(2); k++)
			{
				std::vector<int> point = { i, j, k };
				gemmi::Position position = sample_points[point];
				r(i, j, k) = grid.interpolate_value(position);
			}
		}
	}

}

// Fill a numpy array from a map from grid points to values
template<typename T>
void fill_array(py::array_t<T> sample_array, std::map<std::vector<int>, T> sample_values)
{

	auto r = sample_array.mutable_unchecked(); 
	for (ssize_t i = 0; i < r.shape(0); i++)
	{
		for (ssize_t j = 0; j < r.shape(1); j++)
		{
			for (ssize_t k = 0; k < r.shape(2); k++)
			{
				std::vector<int> point = { i, j, k };
				T value = sample_values[point];
				r(i, j, k) = value;
			}
		}
	}

}

template<typename T>
std::map<std::vector<int>, std::vector<T>>
get_point_position_map(std::vector<std::vector<int>> points, std::vector<std::vector<T>> positions)
{

	std::map<std::vector<int>, std::vector<T>> points_positions_map;

	for (int index = 0; index < points.size(); index++)
	{
		points_positions_map.insert(std::pair<std::vector<int>, std::vector<T>>(points[index], positions[index]));
	}

	return points_positions_map;

}


void add_sample(py::module& m) {

	m.def("sample",
		[](py::array_t<float> sample_array,
			py::array_t<int> sample_points,
			py::array_t<float> sample_positions,
			gemmi::Grid<float> grid)
		{

			std::map<std::vector<int>, gemmi::Position> sample_positions_map = get_sample_positions(sample_points, sample_positions);

			// std::map<std::vector<int>, T> sample_values = sample(grid, sample_positions);

			fill_array(sample_array, sample_positions_map, grid);


		},
		"sampling a grid from an array of grid points <numpy> and an array of cartesian positions <numpy>"
			);

	m.def("sample_positions",
		[](py::array_t<float> sample_array,
			std::map<std::vector<int>, gemmi::Position> sample_positions_map,
			gemmi::Grid<float> grid)
		{

			fill_array(sample_array, sample_positions_map, grid);

		},
		"Sample a grid from a dictionary of grid points that maps to cartesian positions <gemmi::Position>"
			);
	/*
	m.def("sample_point_positions",
		[](py::array_t<float> sample_array,
			std::map<std::vector<int>, std::vector<float>> sample_positions_map,
			gemmi::Grid<float> grid)
		{

			std::map<std::vector<int>, gemmi::Position> sample_positions = get_sample_positions(sample_positions_map);

			std::map<std::vector<int>, float> sample_values = sample_grid(grid, sample_positions);

			fill_array(sample_array, sample_values);

		},
		"Sample a grid from a dictionary of grid points that maps to cartesian positions"
			);
			*/

	m.def("sample_point_positions",
		[](py::array_t<float> sample_array,
			std::vector<std::vector<int>> points,
			std::vector<std::vector<float>> positions,
			gemmi::Grid<float> grid)
		{

			std::map<std::vector<int>, std::vector<float>> point_positions_map = get_point_position_map(points, positions);

			std::map<std::vector<int>, gemmi::Position> sample_positions = get_sample_positions(point_positions_map);

			std::map<std::vector<int>, float> sample_values = sample_grid(grid, sample_positions);

			fill_array(sample_array, sample_values);

		},
		"Sample a grid from a dictionary of grid points that maps to cartesian positions"
			);
	m.def("test_position",
		[](gemmi::Position position)
		{
			return "Loading grid<float> worked";
		},
		"Test if grids load");

	m.def("test_grid",
		[](gemmi::Grid<float> grid)
		{
			return "Loading grid<float> worked";
		},
		"Test if grids load");
	m.def("test_array",
		[](py::array_t<float> arr)
		{
			return "Loading array_t<float> worked";
		},
		"Test if arrays load");


}

PYBIND11_MODULE(gemmi_tools_python, mg) {
	mg.doc() = "General MacroMolecular I/O";
	mg.attr("__version__") = "N/A";
	add_sample(mg);
	
}

