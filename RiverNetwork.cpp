#include "stdafx.h"
#include "RiverNetwork.h"
#include <random>
#include <cmath>
#include <utility>


#define EPSILON 0.0001

pair<int, int> mapGrid(vec3 pos, double step) {
	int i = std::floor(pos[0] / step);
	int j = std::floor(pos[1] / step);
	return pair<int, int>(i, j);
}


vector<pair<int, int>> branchIndices(RiverBranch* branch, double step) {

	vec3 startP = branch->start->position;
	vec3 endP = branch->end->position;

	pair<int, int> start = mapGrid(startP, step);
	pair<int, int> end = mapGrid(endP, step);

	if (start.first > end.first) {
		std::swap(startP, endP);
		std::swap(start, end);
	}

	vector<pair<int, int>> res;

	// Bresenham's line algorithm
	double deltax = endP[0] - startP[0];
	double deltay = endP[1] - startP[1];
	if (std::abs(deltax) < EPSILON) {
		std::swap(deltax, deltay);
	}
	double derror = std::abs(deltay / deltax);
	double error = 0.0;

	int y = start.second;
	for (int x = start.first; x <= end.first; ++x) {
		res.push_back(pair<int, int>(x, y));
		error += derror;
		while (error >= 0.5) {
			y += (deltay > 0.0 ? 1 : -1);
			error -= 1.0;
		}
	}

	return res;
}


RiverNetwork::RiverNetwork(int w, int h, double e)
	:width(w), height(h), e(e)
{
	numW = std::ceil((double)width / (0.75 * e));
	numH = std::ceil((double)height / (0.75 * e));
	grids.resize(numH * numW, vector<RiverBranch*>());
}

RiverNetwork::~RiverNetwork()
{
	//delete all the nodes
	for (int i = 0; i < nodes.size(); i++)
	{
		delete nodes[i];
	}
}

//select the nodes to start from
//here is the simplified version, where we choose a random position on each edge
//the initial nodes all have priority of 1, which is the lowest priority
void RiverNetwork::initialNode()
{
	//create random real numbers between [0,1]
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 1.0);
	//multiply those random numbers by actual dimensions to get random positions on edges
	double ratio1 = dis(gen);
	double l1 = width * ratio1;
	double ratio2 = dis(gen);
	double l2 = height * ratio2;
	double ratio3 = dis(gen);
	double l3 = width * ratio3;
	double ratio4 = dis(gen);
	double l4 = height * ratio4;

	//randomly determine the starting priorities
	int p[4];
	for (int i = 0; i < 4; i++)
	{
		double neta = dis(gen);
		if (neta >= 0.7) {
			p[i] = 5;
		}
		else {
			p[i] = 4;
		}
	}
	//create 4 mouths around the boundary
	RiverNode* mouth1 = new RiverNode(p[0], vec3(l1, 0, 0), nullptr);
	nodes.push_back(mouth1);
	RiverNode* mouth2 = new RiverNode(p[1], vec3((double)width, l2, 0), nullptr);
	nodes.push_back(mouth2);
	RiverNode* mouth3 = new RiverNode(p[2], vec3(l3, (double)height, 0), nullptr);
	nodes.push_back(mouth3);
	RiverNode* mouth4 = new RiverNode(p[3], vec3(0, l4, 0), nullptr);
	nodes.push_back(mouth4);
	//we first apply a continuation for the initial mouths
	RiverNode* mouth11 = new RiverNode(p[0], vec3(l1, e, 0), mouth1);
	nodes.push_back(mouth11);
	nonTerminalNodes.push_back(mouth11);
	RiverNode* mouth22 = new RiverNode(p[1], vec3((double)width - e, l2, 0), mouth2);
	nodes.push_back(mouth22);
	nonTerminalNodes.push_back(mouth22);
	RiverNode* mouth33 = new RiverNode(p[2], vec3(l3, (double)height - e, 0), mouth3);
	nodes.push_back(mouth33);
	nonTerminalNodes.push_back(mouth33);
	RiverNode* mouth44 = new RiverNode(p[3], vec3(e, l4, 0), mouth4);
	nodes.push_back(mouth44);
	nonTerminalNodes.push_back(mouth44);

	std::cout << "Ok in initial node" << std::endl;
}

//from all the non-terminal nodes, select exactly one node that is subject to expansion
//based on the elevationRange and priorities
RiverNode * RiverNetwork::selectNode(double elevationRange)
{
	//Todo
	//loop in all the non-terminal nodes, find the one that has the highest priority that lies within the 
	//elevation range of [z, z+elevationRange]

	if (nonTerminalNodes.size() == 0)
	{
		return nullptr;
	}
	
	vector<RiverNode*> candidateNodes;
	//find the nodes within [z, z+elevationRange]
	for (int i = 0; i < nonTerminalNodes.size(); i++)
	{
		if (nonTerminalNodes[i]->position[2] <= minElevation + elevationRange)
			candidateNodes.push_back(nonTerminalNodes[i]);
	}
	//find the highest priority value in candidateNodes
	int maxP = candidateNodes[0]->priority;
	for (int i = 0; i < candidateNodes.size(); i++)
	{
		if (candidateNodes[i]->priority >= maxP) maxP = candidateNodes[i]->priority;
	}
	//find the set of the nodes with the highest priority value
	vector<RiverNode*> finalcandidateNodes;
	for (int i = 0; i < candidateNodes.size(); i++)
	{
		if (candidateNodes[i]->priority == maxP)
			finalcandidateNodes.push_back(candidateNodes[i]);
	}
	//if this set has more than one element, randomly select one for the final candidate node
	if (finalcandidateNodes.size() > 1)
	{
		std::random_device rdInt;
		std::mt19937 gen(rdInt());
		std::uniform_int_distribution<> dis(0, finalcandidateNodes.size() - 1);
		//get the final selection
		int finalIndex = dis(gen);
		//return the final node selected
		return finalcandidateNodes[finalIndex];
	}
	else {
		return finalcandidateNodes[0];
	}

}

//given the selected candidate node, choose from three different situations
//(symmetric/asymmetric/continuation)
//and compute new branches from them
void RiverNetwork::expandNode(RiverNode * node)
{

	double prob = (double)std::rand() / (double)RAND_MAX;
	// symmetric
	if (prob >= 0.0 && prob < 0.0) {
		int num = 2;
		while (num) {
			int k = 0;
			double initialAngle = 45.0 + 90.0 * (num % 2), currentAngle = initialAngle;
			double angleStep = 2.5;
			RiverNode* newNode = nullptr;
			RiverBranch* branch = nullptr;
			//do {
			//	currentAngle = currentAngle + pow(-1, k) * (k/2) * angleStep;
			//	newNode = getCandidate(node, currentAngle, node->priority - 1);
			//	k++;
			//} while (!validateNode(node, e * 0.25, branch) && currentAngle >= 0.0 && currentAngle <= 180.0);

			//node->children.push_back(newNode);
			//nodes.push_back(newNode);
			//currentAngle >= -2.5 && <= 182.5 means that it can surpass the bottom line of 0/180 degree for once.
			for (int k = 0; currentAngle >= 0 - angleStep && currentAngle <= 180 + angleStep; k++)
			{
				currentAngle = currentAngle + pow(-1, k) * (k / 2) * angleStep;
				int newP = node->priority - 1;
				if (newP < 1)newP = 1;
				newNode = getCandidate(node, currentAngle, newP);
				//if a new node is avaliable at some position, add this node to the node list 
				//also add this node to its parent's children list
				if (validateNode(newNode, e * 0.25, branch))
				{
					std::cout << "newnode P in s" << newNode->priority << endl;
					node->children.push_back(newNode);
					nodes.push_back(newNode);
					//add this newNode to nonterminal 
					nonTerminalNodes.push_back(newNode);
					break;
				}
			}
			num--;
		}
	}
	// asymmetric
	else if (prob > 0.0 && prob <= 1.9) {
		int num = 2;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<> dis(0.6,1.0);
		double ratio = dis(gen);
		int newP = (int)(ratio * node->priority);
		if (newP < 1)newP = 1;
		//set a random priority for the second node
		int p[2] = { node->priority, newP};
		std::cout << "newP" << newP << endl;
		while (num) {
			//int k = 0;
			double initialAngle = 45.0 + 90.0 * (num % 2), currentAngle = initialAngle;
			double angleStep = 2.5;
			RiverNode* newNode = nullptr;
			RiverBranch* branch = nullptr;


			//do {
			//	currentAngle = currentAngle + pow(-1, k) * (k/2) * angleStep;
			//	newNode = getCandidate(node, currentAngle, p[num]);
			//	k++;

			//} while (!validateNode(node, e * 0.25, branch) && currentAngle >= 0.0 && currentAngle <= 180.0);

			//currentAngle >= -2.5 && <= 182.5 means that it can surpass the bottom line of 0/180 degree for once.
			for (int k = 0; currentAngle >= 0 - angleStep && currentAngle <= 180 + angleStep; k++)
			{
				currentAngle = currentAngle + pow(-1, k) * (k / 2) * angleStep;
				newNode = getCandidate(node, currentAngle, p[num-1]);
				//if a new node is avaliable at some position, add this node to the node list 
				//also add this node to its parent's children list
				if (validateNode(newNode, e * 0.25, branch))
				{
					std::cout << "newnode P in as" << newNode->priority << endl;
					node->children.push_back(newNode);
					nodes.push_back(newNode);
					//add this to nonterminal 
					nonTerminalNodes.push_back(newNode);
					break;
				}
			}
			num--;
		}
	}
	// continuation
	else {

		int k = 0;
		double initialAngle = 90.0, currentAngle = initialAngle;
		double angleStep = 2.5;
		RiverNode* newNode = nullptr;
		RiverBranch* branch = nullptr;

		//do {
		//	currentAngle = currentAngle + pow(-1, k) * k * angleStep;
		//	newNode = getCandidate(node, currentAngle, node->priority);

		//} while (!validateNode(node, e * 0.25, branch) && currentAngle >= 0.0 && currentAngle <= 180.0);
		//node->children.push_back(newNode);
		//nodes.push_back(newNode);

		//currentAngle >= -2.5 && <= 182.5 means that it can surpass the bottom line of 0/180 degree for once.
		for (int k = 0; currentAngle >= 0 - angleStep && currentAngle <= 180 + angleStep; k++)
		{
			currentAngle = currentAngle + pow(-1, k) * (k / 2) * angleStep;
			newNode = getCandidate(node, currentAngle, node->priority);
			//if a new node is avaliable at some position, add this node to the node list 
			//also add this node to its parent's children list
			if (validateNode(newNode, e * 0.25, branch))
			{
				std::cout << "newnode P in c" << newNode->priority << endl;
				node->children.push_back(newNode);
				nodes.push_back(newNode);
				//add this to nonterminal 
				nonTerminalNodes.push_back(newNode);
				break;
			}
		}
	}
	// After expansion part, we need to set this node to terminal(remove it from nonTerminals
	int ind = 0;
	for (int i = 0; i < nonTerminalNodes.size(); i++)
	{
		if (nonTerminalNodes[i] == node) {
			ind = i;
			break;
		}
	}
	nonTerminalNodes.erase(nonTerminalNodes.begin() + ind);

}

RiverNode* RiverNetwork::getCandidate(RiverNode* node, double angle, int p) {
	vec2 ppos = vec2(node->parent->position[0], node->parent->position[1]);
	vec2 pos = vec2(node->position[0], node->position[1]);
	vec2 dir = pos - ppos;
	dir = dir.Normalize();
	vec2 perpen = vec2(dir[1], -dir[0]);
	vec2 FinalDir = dir * std::sin(Deg2Rad * angle) + perpen * std::cos(Deg2Rad * angle);
	//double dx = e * std::cos(Deg2Rad * angle), dy = e * std::sin(Deg2Rad * angle);
	return new RiverNode(p, vec3(pos[0] + e * FinalDir[0], pos[1] + e * FinalDir[1], node->position[2] /*+ elevation*/), node);
}


//In this function, the input is a possible new node to be added to the network
//this function check if this node is far enough from:
//1.the boundary of terrain and 
//2.all other branches


bool RiverNetwork::validateNode(RiverNode * node, double boundary, RiverBranch* branch)
{


	// check elevation correctness
	if (node->position[2] < node->parent->position[2]) {
		return false;
	}

	// check if within boundary
	if (node->position[0] < boundary &&
		node->position[0] > (double)width - boundary &&
		node->position[1] < boundary &&
		node->position[1] > (double)height - boundary) {
		return false;
	}

	// check collision with other branches
	branch = new RiverBranch(node->parent, node);
	vector<pair<int, int>> indices = branchIndices(branch, e * 0.75);
	for (auto id : indices) {
		/*for (int i = -1; i < 2; ++i) {
		for (int j = -1; j < 2; ++j) {
		if (id.first + i < 0 || id.first + i >= numW ||
		id.second + j < 0 || id.second + j >= numH) continue;
		if (grids[(id.first + i) * numH + id.second + j].size() != 0) {
		branch = nullptr;
		return false;
		}
		}
		}*/
//		std::cout << id.first << " " << id.second << endl;
		int idx = id.first * numW + id.second;
		if (id.first >= 0 && id.first < numH &&
			id.second >= 0 && id.second < numW) 
		{
			if (grids[idx].size() != 0)
			{
				branch = nullptr;
				return false;
			}

		}
		else {
			branch = nullptr;
			return false;
		}
	}

	branches.push_back(branch);
	for (auto id : indices) {
		grids[id.first * numH + id.second].push_back(branch);
	}

	return true;

}


//Voronoi cell class

//Cell constructor
Cell::Cell()
{
	//Todo
}

//Cell destructor
Cell::~Cell()
{
	//Todo
}

//get the area of current cell
float Cell::getArea()
{
	//Todo
	return 0.0f;
}
