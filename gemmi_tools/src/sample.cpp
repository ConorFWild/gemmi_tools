#incude "gemmi/grid.cpp"


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

template<typename T>
GridBase<T> fill_grid(GridBase<T> base_grid, std::map<int[3], T> sampled_values)
{
	std::map<Point, Position>::iterator it = sampled_values.begin();
	
	for (std::pair<Point, Position> element : sampled_values) {

		sampled_value = base_grid.interpolate_value(Position);

		
	}
}

template<typename T>
void fill_array(py::array_t<T> sample_array, std::map<std::vector<int>, gemmi::Grid<T>::Point> sample_points)
{

	auto r = sample_array.mutable_unchecked<3>(); // Will throw if ndim != 3 or flags.writeable is false
	for (ssize_t i = 0; i < r.shape(0); i++)
	{
		for (ssize_t j = 0; j < r.shape(1); j++)
		{
			for (ssize_t k = 0; k < r.shape(2); k++)
			{
				std::vector<int> point = {i, j, k};
				Position = sample_points[point];
				r(i, j, k) = base_grid.interpolate_value(const Position & ctr);
			}
		}
	}

	return 
}

template<typename T>
GridBase<T> sample(GridBase<T> base_grid, py:array_t<T> sample_array, py:array<T> sample_points_array) {

	std::map<std::vector<int>, gemmi::Grid<T>::Point> sample_positions = get_sample_points(sample_points_array);

	py:array_t<T> grid = fill_grid(sample_array, sample_positions);

	return grid;

}