#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <cstdlib>
using namespace cv;
using namespace std;

template<typename _Tp, typename _DotTp>
static int Sklansky_( Point_<_Tp>** array, int start, int end, int* stack, int nsign, int sign2 )
{
	int incr = end > start ? 1 : -1;
	int pprev = start, pcur = pprev + incr, pnext = pcur + incr;
	int stacksize = 3;

	if( start == end ||
	   (array[start]->x == array[end]->x &&
		array[start]->y == array[end]->y) )
	{
		stack[0] = start;
		return 1;
	}

	stack[0] = pprev;
	stack[1] = pcur;
	stack[2] = pnext;

	end += incr;

	while( pnext != end )
	{
		_Tp cury = array[pcur]->y;
		_Tp nexty = array[pnext]->y;
		_Tp by = nexty - cury;

		if( CV_SIGN( by ) != nsign )
		{
			_Tp ax = array[pcur]->x - array[pprev]->x;
			_Tp bx = array[pnext]->x - array[pcur]->x;
			_Tp ay = cury - array[pprev]->y;
			_DotTp convexity = (_DotTp)ay*bx - (_DotTp)ax*by;

			if( CV_SIGN( convexity ) == sign2 && (ax != 0 || ay != 0) )
			{
				pprev = pcur;
				pcur = pnext;
				pnext += incr;
				stack[stacksize] = pnext;
				stacksize++;
			}
			else
			{
				if( pprev == start )
				{
					pcur = pnext;
					stack[1] = pcur;
					pnext += incr;
					stack[2] = pnext;
				}
				else
				{
					stack[stacksize-2] = pnext;
					pcur = pprev;
					pprev = stack[stacksize-4];
					stacksize--;
				}
			}
		}
		else
		{
			pnext += incr;
			stack[stacksize-1] = pnext;
		}
	}

	return --stacksize;
}


template<typename _Tp>
struct CHullCmpPoints
{
	bool operator()(const Point_<_Tp>* p1, const Point_<_Tp>* p2) const
	{
		if( p1->x != p2->x )
			return p1->x < p2->x;
		if( p1->y != p2->y )
			return p1->y < p2->y;
		return p1 < p2;
	}
};


void convexHull( InputArray _points, OutputArray _hull, bool clockwise, bool returnPoints )
{
	CV_Assert(_points.getObj() != _hull.getObj());
	Mat points = _points.getMat();
	int i, total = points.checkVector(2), depth = points.depth(), nout = 0;
	int miny_ind = 0, maxy_ind = 0;
	CV_Assert(total >= 0 && (depth == CV_32F || depth == CV_32S));

	if( total == 0 )
	{
		_hull.release();
		return;
	}

	returnPoints = !_hull.fixedType() ? returnPoints : _hull.type() != CV_32S;

	bool is_float = depth == CV_32F;
	AutoBuffer<Point*> _pointer(total);
	AutoBuffer<int> _stack(total + 2), _hullbuf(total);
	Point** pointer = &_pointer[0];
	Point2f** pointerf = (Point2f**)pointer;
	Point* data0 = points.ptr<Point>();
	int* stack = &_stack[0];
	int* hullbuf = &_hullbuf[0];

	CV_Assert(points.isContinuous());

	for( i = 0; i < total; i++ )
		pointer[i] = &data0[i];

	if( !is_float )
	{
		std::sort(pointer, pointer + total, CHullCmpPoints<int>());
		for( i = 1; i < total; i++ )
		{
			int y = pointer[i]->y;
			if( pointer[miny_ind]->y > y )
				miny_ind = i;
			if( pointer[maxy_ind]->y < y )
				maxy_ind = i;
		}
	}
	else
	{
		std::sort(pointerf, pointerf + total, CHullCmpPoints<float>());
		for( i = 1; i < total; i++ )
		{
			float y = pointerf[i]->y;
			if( pointerf[miny_ind]->y > y )
				miny_ind = i;
			if( pointerf[maxy_ind]->y < y )
				maxy_ind = i;
		}
	}

	if( pointer[0]->x == pointer[total-1]->x &&
		pointer[0]->y == pointer[total-1]->y )
	{
		hullbuf[nout++] = 0;
	}
	else
	{
		int *tl_stack = stack;
		int tl_count = !is_float ?
			Sklansky_<int, int64>( pointer, 0, maxy_ind, tl_stack, -1, 1) :
			Sklansky_<float, double>( pointerf, 0, maxy_ind, tl_stack, -1, 1);
		int *tr_stack = stack + tl_count;
		int tr_count = !is_float ?
			Sklansky_<int, int64>( pointer, total-1, maxy_ind, tr_stack, -1, -1) :
			Sklansky_<float, double>( pointerf, total-1, maxy_ind, tr_stack, -1, -1);

		if( !clockwise )
		{
			std::swap( tl_stack, tr_stack );
			std::swap( tl_count, tr_count );
		}

		for( i = 0; i < tl_count-1; i++ )
			hullbuf[nout++] = int(pointer[tl_stack[i]] - data0);
		for( i = tr_count - 1; i > 0; i-- )
			hullbuf[nout++] = int(pointer[tr_stack[i]] - data0);
		int stop_idx = tr_count > 2 ? tr_stack[1] : tl_count > 2 ? tl_stack[tl_count - 2] : -1;

		int *bl_stack = stack;
		int bl_count = !is_float ?
			Sklansky_<int, int64>( pointer, 0, miny_ind, bl_stack, 1, -1) :
			Sklansky_<float, double>( pointerf, 0, miny_ind, bl_stack, 1, -1);
		int *br_stack = stack + bl_count;
		int br_count = !is_float ?
			Sklansky_<int, int64>( pointer, total-1, miny_ind, br_stack, 1, 1) :
			Sklansky_<float, double>( pointerf, total-1, miny_ind, br_stack, 1, 1);

		if( clockwise )
		{
			std::swap( bl_stack, br_stack );
			std::swap( bl_count, br_count );
		}

		if( stop_idx >= 0 )
		{
			int check_idx = bl_count > 2 ? bl_stack[1] :
			bl_count + br_count > 2 ? br_stack[2-bl_count] : -1;
			if( check_idx == stop_idx || (check_idx >= 0 &&
										  pointer[check_idx]->x == pointer[stop_idx]->x &&
										  pointer[check_idx]->y == pointer[stop_idx]->y) )
			{
				bl_count = MIN( bl_count, 2 );
				br_count = MIN( br_count, 2 );
			}
		}

		for( i = 0; i < bl_count-1; i++ )
			hullbuf[nout++] = int(pointer[bl_stack[i]] - data0);
		for( i = br_count-1; i > 0; i-- )
			hullbuf[nout++] = int(pointer[br_stack[i]] - data0);

		if( nout >= 3 )
		{
			int min_idx = 0, max_idx = 0, lt = 0;
			for( i = 1; i < nout; i++ )
			{
				int idx = hullbuf[i];
				lt += hullbuf[i-1] < idx;
				if( lt > 1 && lt <= i-2 )
					break;
				if( idx < hullbuf[min_idx] )
					min_idx = i;
				if( idx > hullbuf[max_idx] )
					max_idx = i;
			}
			int mmdist = std::abs(max_idx - min_idx);
			if( (mmdist == 1 || mmdist == nout-1) && (lt <= 1 || lt >= nout-2) )
			{
				int ascending = (max_idx + 1) % nout == min_idx;
				int i0 = ascending ? min_idx : max_idx, j = i0;
				if( i0 > 0 )
				{
					for( i = 0; i < nout; i++ )
					{
						int curr_idx = stack[i] = hullbuf[j];
						int next_j = j+1 < nout ? j+1 : 0;
						int next_idx = hullbuf[next_j];
						if( i < nout-1 && (ascending != (curr_idx < next_idx)) )
							break;
						j = next_j;
					}
					if( i == nout )
						memcpy(hullbuf, stack, nout*sizeof(hullbuf[0]));
				}
			}
		}
	}

	if( !returnPoints )
		Mat(nout, 1, CV_32S, hullbuf).copyTo(_hull);
	else
	{
		_hull.create(nout, 1, CV_MAKETYPE(depth, 2));
		Mat hull = _hull.getMat();
		size_t step = !hull.isContinuous() ? hull.step[0] : sizeof(Point);
		for( i = 0; i < nout; i++ )
			*(Point*)(hull.ptr() + i*step) = data0[hullbuf[i]];
	}
}

int main(int argc, char** argv)
{
	string image_path;

	if(argc < 2)
	{
		cout << "No Filename Given!" << endl;
		return 0;
	}
	else
		image_path = argv[1];

	Mat src, gray, blur_image, threshold_output;

	src = imread(image_path, 1);

	cvtColor(src, gray, COLOR_BGR2GRAY);

	blur(gray, blur_image, Size(3, 3));

	threshold(gray, threshold_output, 200, 255, THRESH_BINARY);

	namedWindow("Source", WINDOW_AUTOSIZE);
	imshow("Source", src);

	Mat src_copy = src.clone();

	vector< vector<Point> > contours;
	vector<Vec4i> hierarchy;

	findContours(threshold_output, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));

	vector< vector<Point> > hull(contours.size());

	for(int i = 0; i < contours.size(); i++)
		convexHull(Mat(contours[i]), hull[i], false);

	Mat inter = Mat::zeros(threshold_output.size(), CV_8UC3);
	Mat drawing = Mat::zeros(threshold_output.size(), CV_8UC3);

	for(int i = 0; i < contours.size(); i++)
	{
		Scalar color_contours = Scalar(0, 255, 0);
		Scalar color = Scalar(255, 255, 255);
		drawContours(inter, contours, i, color_contours, 2, 8, vector<Vec4i>(), 0, Point());
		drawContours(drawing, contours, i, color_contours, 2, 8, vector<Vec4i>(), 0, Point());
		drawContours(drawing, hull, i, color, 2, 8, vector<Vec4i>(), 0, Point());
	}

	namedWindow("Intermediate", WINDOW_AUTOSIZE);
	imshow("Intermediate", inter);

	namedWindow("Output", WINDOW_AUTOSIZE);
	imshow("Output", drawing);

	waitKey(0);
}