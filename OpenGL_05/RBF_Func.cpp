#include "RBF_Func.h"

#include <cmath>
#include <Eigen\Dense>

// for debug
#include <iostream>
using std::cout;
using std::endl;

RBF_Func::RBF_Func() :
	mBBoxList(NULL)
{

}

// init from vector<vec3>
RBF_Func::RBF_Func(std::vector<vec3> & points)
{
	RBF_PointNormal_List *pointNormals = initPoints(points);
	initBBox(pointNormals);
	initFunc();
}

// init from file
// format vector3
RBF_Func::RBF_Func(const string & path)
{
	RBF_PointNormal_List *pointNormals = loadPoints(path.c_str());
	initBBox(pointNormals);
	initFunc();
	//test();
}

RBF_Func::~RBF_Func()
{

}

// the final sectioned function
float RBF_Func::func(float x, float y, float z)
{
	float value = 0.0f;
	bool haveBBox = false;

	RBF_BBox_Iter boxI = mBBoxList.begin();
	for (; boxI != mBBoxList.end(); boxI++)
	{
		if (x >= (*boxI)->xMin && x < (*boxI)->xMax)
		{
			// each Bounding Box multi by it's weight
			return bBoxFunc(*boxI, x, y, z);// *bBoxFuncWeight(*boxI, x);
			haveBBox = true;
		}
	}

	if (haveBBox)
		return value;
	return 1.0f;
}

// load points from vector<vec3>
RBF_PointNormal_List * RBF_Func::initPoints(std::vector<vec3> & points)
{
	PointAndNormal *newPoint;
	RBF_PointNormal_List *pointNormals = new RBF_PointNormal_List;

	std::vector<vec3>::iterator pointI = points.begin();
	for (; pointI < points.end(); pointI++)
	{
		newPoint = new PointAndNormal;
		newPoint->point = vec3(*pointI);
		pointNormals->push_back(newPoint);
	}

	return pointNormals;
}

// load points from file
// format vector3
RBF_PointNormal_List * RBF_Func::loadPoints(const string & path)
{
	std::ifstream fileIn(path);
	std::stringstream ss;
	string line;
	RBF_PointNormal_List *pointNormals = new RBF_PointNormal_List;
	PointAndNormal *newPoint = new PointAndNormal;

	while (!fileIn.eof())
	{
		std::getline(fileIn, line);
		if (line.empty())
			continue;
		ss.str(line);
		ss.clear();

		newPoint = new PointAndNormal;
		ss >> newPoint->point.x >> newPoint->point.y >> newPoint->point.z;
		pointNormals->push_back(newPoint);
	}

	fileIn.close();

	return pointNormals;
}

// make num of points in one bounding box less than BBOX_MAX_POINTS
void RBF_Func::initBBox(RBF_PointNormal_List * pointNormals)
{
	BoundingBox *box = getBBox(pointNormals);

	box->xMin = INFINITY_F;
	box->xMax = -INFINITY_F;

	// get the Min and Max x value
	RBF_Point_Iter pointI = box->points.begin();
	for (; pointI != box->points.end(); pointI++)
	{
		if (box->xMin > (*pointI)->point.x)
			box->xMin = (*pointI)->point.x;
		if (box->xMax < (*pointI)->point.x)
			box->xMax = (*pointI)->point.x;
	}
	box->xMax += BOUNDING_BOX_THICK;
	box->xMin -= BOUNDING_BOX_THICK;
	bBoxInfo(box);

	// cut the box to a list by x
	cutBBox(box);

	// sort by xMax
	mBBoxList.sort(bBoxCmp);

	// merge two near Bounding Box
	// still some trouble with this
	RBF_BBox_Iter boxI_i = mBBoxList.begin();
	RBF_BBox_Iter boxI_j = mBBoxList.begin();
	for (boxI_i++; boxI_i != mBBoxList.end(); boxI_i++, boxI_j++)
		mergeBBox(*boxI_j, *boxI_i);

	// update Bounding Box's Half Width and xMid
	RBF_BBox_Iter boxI = mBBoxList.begin();
	for (; boxI != mBBoxList.end(); boxI++)
		bBoxInfo(*boxI);
}

void RBF_Func::initFunc()
{
	// solve the weights of each point
	RBF_BBox_Iter boxI = mBBoxList.begin();
	for (; boxI != mBBoxList.end(); boxI++)
		bBoxPointsWeight(*boxI);
}

// get the normal and add some points to the Bounding Box
// still some trouble with this
// ================================================================================
BoundingBox * RBF_Func::getBBox(RBF_PointNormal_List * pointNormals)
{
	BoundingBox *box = new BoundingBox;
	float pointDistanceMin = INFINITY_F;
	float **pointDistanceList = new float *[pointNormals->size()];
	PointAndNormal **pointNormalIndexs = new PointAndNormal *[pointNormals->size()];

	RBF_PointNormal_Iter pointNormalI_i = pointNormals->begin();
	RBF_PointNormal_Iter pointNormalI_j;
	for (int i = 0; pointNormalI_i != pointNormals->end(); pointNormalI_i++, i++)
	{
		pointNormalIndexs[i] = *pointNormalI_i;
		pointDistanceList[i] = new float[pointNormals->size()];

		pointNormalI_j = pointNormalI_i;
		pointNormalI_j++;
		for (int j = i + 1; pointNormalI_j != pointNormals->end(); pointNormalI_j++, j++)
		{
			pointDistanceList[i][j] = glm::distance((*pointNormalI_i)->point, (*pointNormalI_j)->point);

			if (pointDistanceMin > pointDistanceList[i][j])
				pointDistanceMin = pointDistanceList[i][j];
		}
	}

	getPointNormal(pointNormalIndexs, pointDistanceList, pointNormals->size(), pointDistanceMin);
	/*
	for (int i = 0; i < pointNormals->size(); i++)
	{
	cout << pointNormalIndexs[i]->point.x << "\t"
	<< pointNormalIndexs[i]->point.y << "\t"
	<< pointNormalIndexs[i]->point.z << "\t"
	<< endl;

	cout << pointNormalIndexs[i]->normal.x << "\t"
	<< pointNormalIndexs[i]->normal.y << "\t"
	<< pointNormalIndexs[i]->normal.z << "\t"
	<< endl << endl;
	}
	*/
	addNewPoints(pointNormals, box, 0.02f);

	return box;
}

// cut the Bounding Box to a list by x
void RBF_Func::cutBBox(BoundingBox * box)
{
	// until small enough
	if (box->points.size() < BBOX_MAX_POINTS
		|| box->xHalfWidth < BBOX_MIN_HALF_WIDTH)
	{
		if (box->points.size() > 0)
		{
			mBBoxList.push_back(box);
		}
		return;
	}

	BoundingBox *newBBox = new BoundingBox;
	divBBoxByX(box, newBBox);
	bBoxInfo(newBBox);

	// cut two new Bounding Box
	cutBBox(box);
	cutBBox(newBBox);
}

// get the normal of each point
// still some trouble with this
// ===============================================================================================
void RBF_Func::getPointNormal(PointAndNormal ** pointNormalIndexs, float ** pointDistanceList, int numOfPoints, float disMin)
{
	PointAndNormal * pointInPlane[3];
	vec3 vector1, vector2;
	vec3 normal;
	float pointDistance = 100.0f * disMin;
	int k;

	for (int i = 0; i < numOfPoints; i++)
	{
		/*
		k = 0;
		for (int j = 0; k < 3 && j < numOfPoints; j++)
		{
		if ( (i < j && pointDistanceList[i][j] < pointDistance)
		|| (i > j && pointDistanceList[j][i] < pointDistance) )
		pointInPlane[k++] = pointNormalIndexs[j];
		}

		if (k < 3)
		continue;

		vector1 = pointInPlane[0]->point - pointInPlane[1]->point;
		vector2 = pointInPlane[1]->point - pointInPlane[2]->point;
		normal = glm::cross(vector1, vector2);
		*/
		normal = pointNormalIndexs[i]->point;
		pointNormalIndexs[i]->normal = vec3(glm::normalize(normal));
	}
}

void RBF_Func::addNewPoints(RBF_PointNormal_List * pointNormals, BoundingBox * box, float pointDistance)
{
	vec3 *point, *normal;
	vec3 point_temp;

	RBF_PointNormal_Iter pointNormalI = pointNormals->begin();
	for (; pointNormalI != pointNormals->end(); pointNormalI++)
	{
		point = &((*pointNormalI)->point);
		normal = &((*pointNormalI)->normal);
		box->points.push_back(new RBF_Point({ *point, 0.0f, 0.0f }));

		box->points.push_back(new RBF_Point({ (*point) + (*normal) * pointDistance, 0.0f, 1.0f }));

		box->points.push_back(new RBF_Point({ (*point) - (*normal) * pointDistance, 0.0f, -1.0f }));

		/* for debug
		point_temp = *point;
		cout << point_temp.x << "\t"
			<< point_temp.y << "\t"
			<< point_temp.z << "\t"
			<< endl;
		
		point_temp = (*point) + (*normal) * pointDistance;
		cout << point_temp.x << "\t"
			<< point_temp.y << "\t"
			<< point_temp.z << "\t"
			<< endl;

		point_temp = (*point) - (*normal) * pointDistance;
		cout << point_temp.x << "\t"
			<< point_temp.y << "\t"
			<< point_temp.z << "\t"
			<< endl << endl;
		*/
	}
}

void RBF_Func::divBBoxByX(BoundingBox * box1, BoundingBox * box2)
{
	float xMid = (box1->xMax + box1->xMin) / 2.0f;
	box2->xMax = box1->xMax;
	box2->xMin = xMid - BOUNDING_BOX_THICK;
	box1->xMax = xMid + BOUNDING_BOX_THICK;

	RBF_Point_Iter pointI = box1->points.begin();
	for (; pointI != box1->points.end(); )
	{
		if ((*pointI)->point.x < xMid)
		{
			pointI++;
			continue;
		}

		box2->points.push_back(*pointI);
		pointI = box1->points.erase(pointI);
	}
}

void RBF_Func::mergeBBox(BoundingBox * box1, BoundingBox * box2)
{
	RBF_Point *newPoint;
	box1->xMax = box2->xMax;

	RBF_Point_Iter pointI = box2->points.begin();
	for (; pointI != box2->points.end(); pointI++)
	{
		newPoint = new RBF_Point(**pointI);
		box1->points.push_back(newPoint);
	}
}

void RBF_Func::bBoxPointsWeight(BoundingBox * box)
{
	int numOfPoints = box->points.size();
	Eigen::MatrixXf matrix = Eigen::MatrixXf::Zero(numOfPoints + 3, numOfPoints + 3);
	Eigen::VectorXf d = Eigen::VectorXf::Zero(numOfPoints + 3);

	RBF_Point_Iter pointI_i = box->points.begin();
	RBF_Point_Iter pointI_j;
	for (int i = 0; i < numOfPoints; i++, pointI_i++)
	{
		pointI_j = box->points.begin();
		for (int j = 0; j < numOfPoints; j++, pointI_j++)
		{
			matrix(i, j) = vec3DisModuleCube((*pointI_i)->point, (*pointI_j)->point);
		}

		matrix(i, numOfPoints) = (*pointI_i)->point.x;
		matrix(i, numOfPoints + 1) = (*pointI_i)->point.y;
		matrix(i, numOfPoints + 2) = (*pointI_i)->point.z;
		//matrix(i, numOfPoints + 3) = 1.0f;
	}

	pointI_i = box->points.begin();
	for (int i = 0; i < numOfPoints; i++, pointI_i++)
	{
		matrix(numOfPoints, i) = (*pointI_i)->point.x;
		matrix(numOfPoints + 1, i) = (*pointI_i)->point.y;
		matrix(numOfPoints + 2, i) = (*pointI_i)->point.z;
		//matrix(numOfPoints + 3, i) = 1.0f;

		d(i) = (*pointI_i)->d - 1;
	}

	//cout << matrix << endl;
	//cout << d << endl << endl;

	//Eigen::VectorXf w = matrix.inverse() * d;
	Eigen::VectorXf w = matrix.colPivHouseholderQr().solve(d);
	//Eigen::VectorXf w = matrix.fullPivLu().solve(d);

	//cout << w << endl << endl;
	//cout << w(numOfPoints + 3);

	pointI_i = box->points.begin();
	for (int i = 0; i < numOfPoints; i++, pointI_i++)
		(*pointI_i)->w = w(i);

	box->a = w(numOfPoints);
	box->b = w(numOfPoints + 1);
	box->c = w(numOfPoints + 2);

	/*// for debug
	pointI_i = box->points.begin();
	for (; pointI_i != box->points.end(); pointI_i++)
	cout << func((*pointI_i)->point.x, (*pointI_i)->point.y, (*pointI_i)->point.z) << endl;
	*/

	/*// for debug
	Eigen::VectorXf t1 = matrix * w;
	for (int i = 0; i < numOfPoints; i++)
	{
		cout << d(i) << '\t' << t1(i) << endl;
	}
	*/
	
	static int _temp = 1;
	Eigen::VectorXf t2 = matrix * w - d;
	float tt = t2.transpose() * t2;
	cout << tt << "\t" << box->xMin << "\t" << box->xMax << endl << endl;
	/*
	if (_temp == 52)
	{
		RBF_BBox_Iter boxI = this->mBBoxList.begin();
		for (; boxI != mBBoxList.end(); boxI++)
		{
			if (*boxI == box)
			{
				mBBoxList.erase(boxI);
				break;
			}
		}
	}
	/**/
}

void RBF_Func::bBoxInfo(BoundingBox * box)
{
	box->xMid = (box->xMax + box->xMin) / 2.0f;
	box->xHalfWidth = (box->xMax - box->xMin) / 2.0f;
}

float RBF_Func::bBoxFunc(BoundingBox & box, float x, float y, float z)
{
	return bBoxFunc(&box, x, y, z);
}

float RBF_Func::bBoxFunc(BoundingBox * box, float x, float y, float z)
{
	float value = box->a * x + box->b * y + box->c * z + 1.0f;
	vec3 point(x, y, z);
	float phi;

	RBF_Point_Iter pointI = box->points.begin();
	for (; pointI != box->points.end(); pointI++)
	{
		phi = (*pointI)->w * vec3DisModuleCube(point, (*pointI)->point);
		value += phi;
	}

	return value;
}

float RBF_Func::bBoxFuncWeight(BoundingBox * box, float x)
{
	return 1.0f - fabs(x - box->xMid) / box->xHalfWidth;
}

float RBF_Func::vec3DisModuleCube(const vec3 & point1, const vec3 & point2)
{
	vec3 pd = point1 - point2;

	float module = std::sqrtf(pd.x * pd.x + pd.y * pd.y + pd.z * pd.z);
	return module * module * module;
}

void RBF_Func::test()
{
	cout << endl << "=========test=========" << endl;

	RBF_Point *p;

	RBF_BBox_Iter boxI_2 = mBBoxList.begin();
	RBF_BBox_Iter boxI_1 = boxI_2++;
	RBF_Point_Iter pointI = (*boxI_2)->points.begin();
	for (; pointI != (*boxI_2)->points.end(); pointI++)
	{
		p = *pointI;
		cout << "\tBBox1:\t" << bBoxFunc(*boxI_1, p->point.x, p->point.y, p->point.z);
		cout << "\tBBox2:\t" << bBoxFunc(*boxI_2, p->point.x, p->point.y, p->point.z);
		cout << "\tFunc:\t"  << func(p->point.x, p->point.y, p->point.z);
		cout << endl;
	}
}

bool bBoxCmp(const BoundingBox * box1, const BoundingBox * box2)
{
	return box1->xMin < box2->xMin;
}
