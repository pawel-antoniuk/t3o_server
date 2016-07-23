#include <boost/multi_array.hpp>

namespace t3o
{

class game_board
{
public:
	game_board(unsigned width, unsigned height) :
		_width(width),
		_height(height),
		_board_array(boost::extents[width][height])
	{
	}

	void set_field(unsigned field, unsigned x, unsigned y)
	{
		_board_array[x][y] = field;
	}

	unsigned who_won()
	{
		//TODO: complex algorithm for each board size
		auto& a = _board_array;
		for(int i = 0; i < 3; ++i)
		{
			if(a[i][0] == a[i][1] && a[i][0] == a[i][2])
				return a[i][0];
		}
		for(int i = 0; i < 3; ++i)
		{
			if(a[0][i] == a[1][i] && a[0][i] == a[2][i])
				return a[0][i];
		}
		if(a[0][0] == a[1][1] && a[0][0] == a[2][2])
			return a[0][0];
		if(a[2][0] == a[1][1] && a[2][0] == a[0][2])
			return a[2][0];
		return 0;
	}
	
	void clear()
	{
		std::fill_n(_board_array.data(),
				_board_array.num_elements(), 0);
	}

	auto width() const { return _width; }
	auto height() const { return _height; }

private:
	unsigned _width, _height;
	boost::multi_array<unsigned, 2> _board_array;
};

}
