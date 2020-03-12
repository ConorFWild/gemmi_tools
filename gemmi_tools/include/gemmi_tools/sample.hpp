#include <gemmi/grid.hpp>
#include <gemmi/unitcell.hpp>

//Translate a map<points, positions> to Gemmi a map<point/gemmi Positions>
template<typename T>
std::map<std::vector<int>, gemmi::Position>
get_sample_positions(std::map<std::vector<int>, std::vector<T>> sample_positions)
{

	std::map<std::vector<int>, gemmi::Position> grid_map;

	auto it = sample_positions.begin();

	while (it != sample_positions.end())
	{
		auto location = it->first;
		auto position = it->second;

		gemmi::Position position_gemmi(position[0], position[1], position[2]);

		grid_map.insert(std::pair<std::vector<int>, gemmi::Position>(location, position_gemmi));
	}

	return grid_map;

}

template<typename T>
std::map<std::vector<int>, T> 
sample_grid(gemmi::Grid<T> grid, std::map<std::vector<int>, gemmi::Position> sample_positions)
{

	std::map<std::vector<int>, T> values_map;

	auto it = sample_positions.begin();
	while (it != sample_positions.end())
	{

		auto location = it->first;
		auto position = it->second;

		T grid_value = grid.interpolate_value(position);

		values_map.insert(std::pair<std::vector<int>, T>(location, grid_value));

	}

	return values_map;

}