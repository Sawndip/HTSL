#include "RSDR.h"

#include <algorithm>

#include <assert.h>

#include <iostream>

using namespace sdr;

void RSDR::createRandom(int visibleWidth, int visibleHeight, int hiddenWidth, int hiddenHeight, int receptiveRadius, int inhibitionRadius, int recurrentRadius, float initMinWeight, float initMaxWeight, float initMinInhibition, float initMaxInhibition, float initThreshold, std::mt19937 &generator) {
	std::uniform_real_distribution<float> weightDist(initMinWeight, initMaxWeight);
	std::uniform_real_distribution<float> inhibitionDist(initMinInhibition, initMaxInhibition);

	_visibleWidth = visibleWidth;
	_visibleHeight = visibleHeight;
	_hiddenWidth = hiddenWidth;
	_hiddenHeight = hiddenHeight;

	_receptiveRadius = receptiveRadius;
	_inhibitionRadius = inhibitionRadius;
	_recurrentRadius = recurrentRadius;

	int numVisible = visibleWidth * visibleHeight;
	int numHidden = hiddenWidth * hiddenHeight;
	int receptiveSize = std::pow(receptiveRadius * 2 + 1, 2);
	int inhibitionSize = std::pow(inhibitionRadius * 2 + 1, 2);
	int recurrentSize = std::pow(recurrentRadius * 2 + 1, 2);

	_visible.resize(numVisible);

	_hidden.resize(numHidden);

	float hiddenToVisibleWidth = static_cast<float>(visibleWidth) / static_cast<float>(hiddenWidth);
	float hiddenToVisibleHeight = static_cast<float>(visibleHeight) / static_cast<float>(hiddenHeight);

	for (int hi = 0; hi < numHidden; hi++) {
		int hx = hi % hiddenWidth;
		int hy = hi / hiddenWidth;

		int centerX = std::round(hx * hiddenToVisibleWidth);
		int centerY = std::round(hy * hiddenToVisibleHeight);

		_hidden[hi]._threshold = initThreshold;

		// Receptive
		_hidden[hi]._feedForwardConnections.reserve(receptiveSize);

		for (int dx = -receptiveRadius; dx <= receptiveRadius; dx++)
			for (int dy = -receptiveRadius; dy <= receptiveRadius; dy++) {
				int vx = centerX + dx;
				int vy = centerY + dy;

				if (vx >= 0 && vx < visibleWidth && vy >= 0 && vy < visibleHeight) {
					int vi = vx + vy * visibleWidth;

					ConnectionFeed c;

					c._weight = weightDist(generator);
					c._index = vi;
					
					_hidden[hi]._feedForwardConnections.push_back(c);
				}
			}

		_hidden[hi]._feedForwardConnections.shrink_to_fit();

		// Inhibition
		_hidden[hi]._lateralConnections.reserve(inhibitionSize);

		for (int dx = -inhibitionRadius; dx <= inhibitionRadius; dx++)
			for (int dy = -inhibitionRadius; dy <= inhibitionRadius; dy++) {
				if (dx == 0 && dy == 0)
					continue;

				int hox = hx + dx;
				int hoy = hy + dy;

				if (hox >= 0 && hox < hiddenWidth && hoy >= 0 && hoy < hiddenHeight) {
					int hio = hox + hoy * hiddenWidth;

					ConnectionLateral c;

					c._weight = inhibitionDist(generator);
					c._index = hio;
		
					_hidden[hi]._lateralConnections.push_back(c);
				}
			}

		_hidden[hi]._lateralConnections.shrink_to_fit();

		// Recurrent
		if (recurrentRadius != -1) {
			_hidden[hi]._recurrentConnections.reserve(recurrentSize);

			for (int dx = -recurrentRadius; dx <= recurrentRadius; dx++)
				for (int dy = -recurrentRadius; dy <= recurrentRadius; dy++) {
					if (dx == 0 && dy == 0)
						continue;

					int hox = hx + dx;
					int hoy = hy + dy;

					if (hox >= 0 && hox < hiddenWidth && hoy >= 0 && hoy < hiddenHeight) {
						int hio = hox + hoy * hiddenWidth;

						ConnectionFeed c;

						c._weight = weightDist(generator);
						c._index = hio;

						_hidden[hi]._recurrentConnections.push_back(c);
					}
				}

			_hidden[hi]._recurrentConnections.shrink_to_fit();
		}
	}
}

void RSDR::activate(int subIterSettle, int subIterMeasure, float leak) {
	// Activate
	for (int hi = 0; hi < _hidden.size(); hi++) {
		float centerFF = 0.0f;
		float centerR = 0.0f;

		for (int ci = 0; ci < _hidden[hi]._feedForwardConnections.size(); ci++)
			centerFF += _visible[_hidden[hi]._feedForwardConnections[ci]._index]._input;

		for (int ci = 0; ci < _hidden[hi]._recurrentConnections.size(); ci++)
			centerR += _hidden[_hidden[hi]._recurrentConnections[ci]._index]._statePrev;

		centerFF /= _hidden[hi]._feedForwardConnections.size();
		centerR /= _hidden[hi]._recurrentConnections.size();

		float sum = 0.0f;

		for (int ci = 0; ci < _hidden[hi]._feedForwardConnections.size(); ci++)
			sum += (_visible[_hidden[hi]._feedForwardConnections[ci]._index]._input - centerFF) * _hidden[hi]._feedForwardConnections[ci]._weight;

		for (int ci = 0; ci < _hidden[hi]._recurrentConnections.size(); ci++)
			sum += (_hidden[_hidden[hi]._recurrentConnections[ci]._index]._statePrev - centerR) * _hidden[hi]._recurrentConnections[ci]._weight;

		_hidden[hi]._excitation = sum;

		_hidden[hi]._spike = 0.0f;
		_hidden[hi]._spikePrev = 0.0f;
		_hidden[hi]._activation = 0.0f;
		_hidden[hi]._state = 0.0f;
	}

	// Inhibit
	float subIterMeasureInv = 1.0f / subIterMeasure;

	for (int iter = 0; iter < subIterSettle; iter++) {
		for (int hi = 0; hi < _hidden.size(); hi++) {
			float inhibition = 0.0f;

			for (int ci = 0; ci < _hidden[hi]._lateralConnections.size(); ci++)
				inhibition += _hidden[hi]._lateralConnections[ci]._weight * _hidden[_hidden[hi]._lateralConnections[ci]._index]._spikePrev;

			float activation = (1.0f - leak) * _hidden[hi]._activation + _hidden[hi]._excitation - inhibition;

			if (activation > _hidden[hi]._threshold) {
				_hidden[hi]._spike = 1.0f;

				activation = 0.0f;
			}
			else
				_hidden[hi]._spike = 0.0f;

			_hidden[hi]._activation = activation;
		}

		for (int hi = 0; hi < _hidden.size(); hi++)
			_hidden[hi]._spikePrev = _hidden[hi]._spike;
	}

	for (int iter = 0; iter < subIterMeasure; iter++) {
		for (int hi = 0; hi < _hidden.size(); hi++) {
			float inhibition = 0.0f;

			for (int ci = 0; ci < _hidden[hi]._lateralConnections.size(); ci++)
				inhibition += _hidden[hi]._lateralConnections[ci]._weight * _hidden[_hidden[hi]._lateralConnections[ci]._index]._spikePrev;

			float activation = (1.0f - leak) * _hidden[hi]._activation + _hidden[hi]._excitation - inhibition;

			if (activation > _hidden[hi]._threshold) {
				_hidden[hi]._spike = 1.0f;

				_hidden[hi]._state += subIterMeasureInv;

				activation = 0.0f;
			}
			else
				_hidden[hi]._spike = 0.0f;

			_hidden[hi]._activation = activation;
		}

		for (int hi = 0; hi < _hidden.size(); hi++)
			_hidden[hi]._spikePrev = _hidden[hi]._spike;
	}
}

void RSDR::inhibit(int subIterSettle, int subIterMeasure, float leak, const std::vector<float> &activations, std::vector<float> &states) {
	states.clear();
	states.assign(_hidden.size(), 0.0f);

	for (int hi = 0; hi < _hidden.size(); hi++) {
		_hidden[hi]._excitation = activations[hi];
		_hidden[hi]._spike = 0.0f;
		_hidden[hi]._spikePrev = 0.0f;
		_hidden[hi]._activation = 0.0f;
	}

	// Inhibit
	float subIterMeasureInv = 1.0f / subIterMeasure;

	for (int iter = 0; iter < subIterSettle; iter++) {
		for (int hi = 0; hi < _hidden.size(); hi++) {
			float inhibition = 0.0f;

			for (int ci = 0; ci < _hidden[hi]._lateralConnections.size(); ci++)
				inhibition += _hidden[hi]._lateralConnections[ci]._weight * _hidden[_hidden[hi]._lateralConnections[ci]._index]._spikePrev;

			float activation = (1.0f - leak) * _hidden[hi]._activation + _hidden[hi]._excitation - inhibition;

			if (activation > _hidden[hi]._threshold) {
				_hidden[hi]._spike = 1.0f;

				activation = 0.0f;
			}
			else
				_hidden[hi]._spike = 0.0f;

			_hidden[hi]._activation = activation;
		}

		for (int hi = 0; hi < _hidden.size(); hi++)
			_hidden[hi]._spikePrev = _hidden[hi]._spike;
	}

	for (int iter = 0; iter < subIterMeasure; iter++) {
		for (int hi = 0; hi < _hidden.size(); hi++) {
			float inhibition = 0.0f;

			for (int ci = 0; ci < _hidden[hi]._lateralConnections.size(); ci++)
				inhibition += _hidden[hi]._lateralConnections[ci]._weight * _hidden[_hidden[hi]._lateralConnections[ci]._index]._spikePrev;

			float activation = (1.0f - leak) * _hidden[hi]._activation + _hidden[hi]._excitation - inhibition;

			if (activation > _hidden[hi]._threshold) {
				_hidden[hi]._spike = 1.0f;

				states[hi] += subIterMeasureInv;

				activation = 0.0f;
			}
			else
				_hidden[hi]._spike = 0.0f;

			_hidden[hi]._activation = activation;
		}

		for (int hi = 0; hi < _hidden.size(); hi++)
			_hidden[hi]._spikePrev = _hidden[hi]._spike;
	}
}

void RSDR::learn(float learnFeedForward, float learnRecurrent, float learnLateral, float learnThreshold, float sparsity) {
	std::vector<float> visibleErrors(_visible.size(), 0.0f);
	std::vector<float> hiddenErrors(_hidden.size(), 0.0f);

	for (int vi = 0; vi < _visible.size(); vi++)
		visibleErrors[vi] = _visible[vi]._input - _visible[vi]._reconstruction;

	for (int hi = 0; hi < _hidden.size(); hi++)
		hiddenErrors[hi] = _hidden[hi]._statePrev - _hidden[hi]._reconstruction;

	float sparsitySquared = sparsity * sparsity;

	for (int hi = 0; hi < _hidden.size(); hi++) {
		float learn = _hidden[hi]._state;

		if (learn > 0.0f) {
			for (int ci = 0; ci < _hidden[hi]._feedForwardConnections.size(); ci++)
				_hidden[hi]._feedForwardConnections[ci]._weight += learnFeedForward * learn * (_visible[_hidden[hi]._feedForwardConnections[ci]._index]._input - learn * _hidden[hi]._feedForwardConnections[ci]._weight);

			for (int ci = 0; ci < _hidden[hi]._recurrentConnections.size(); ci++)
				_hidden[hi]._recurrentConnections[ci]._weight += learnRecurrent * learn * (_hidden[_hidden[hi]._recurrentConnections[ci]._index]._statePrev - learn * _hidden[hi]._recurrentConnections[ci]._weight);
		}

		for (int ci = 0; ci < _hidden[hi]._lateralConnections.size(); ci++)
			_hidden[hi]._lateralConnections[ci]._weight = std::max(0.0f, _hidden[hi]._lateralConnections[ci]._weight + learnLateral * (_hidden[hi]._state * _hidden[_hidden[hi]._lateralConnections[ci]._index]._state - sparsitySquared)); //_hidden[_hidden[hi]._lateralConnections[ci]._index]._state * 

		_hidden[hi]._threshold += learnThreshold * (_hidden[hi]._state - sparsity);
	}
}

void RSDR::learn(const std::vector<float> &attentions, float learnFeedForward, float learnRecurrent, float learnLateral, float learnThreshold, float sparsity) {
	std::vector<float> visibleErrors(_visible.size(), 0.0f);
	std::vector<float> hiddenErrors(_hidden.size(), 0.0f);

	for (int vi = 0; vi < _visible.size(); vi++)
		visibleErrors[vi] = _visible[vi]._input - _visible[vi]._reconstruction;

	for (int hi = 0; hi < _hidden.size(); hi++)
		hiddenErrors[hi] = _hidden[hi]._statePrev - _hidden[hi]._reconstruction;

	float sparsitySquared = sparsity * sparsity;

	for (int hi = 0; hi < _hidden.size(); hi++) {
		float learn = _hidden[hi]._state;

		if (learn > 0.0f) {
			for (int ci = 0; ci < _hidden[hi]._feedForwardConnections.size(); ci++)
				_hidden[hi]._feedForwardConnections[ci]._weight += learnFeedForward * attentions[hi] * learn * (_visible[_hidden[hi]._feedForwardConnections[ci]._index]._input - learn * _hidden[hi]._feedForwardConnections[ci]._weight);

			for (int ci = 0; ci < _hidden[hi]._recurrentConnections.size(); ci++)
				_hidden[hi]._recurrentConnections[ci]._weight += learnRecurrent * attentions[hi] * learn * (_hidden[_hidden[hi]._recurrentConnections[ci]._index]._statePrev - learn * _hidden[hi]._recurrentConnections[ci]._weight);
		}

		for (int ci = 0; ci < _hidden[hi]._lateralConnections.size(); ci++)
			_hidden[hi]._lateralConnections[ci]._weight = std::max(0.0f, _hidden[hi]._lateralConnections[ci]._weight + learnLateral * attentions[hi] * (_hidden[hi]._state * _hidden[_hidden[hi]._lateralConnections[ci]._index]._state - sparsitySquared)); //_hidden[_hidden[hi]._lateralConnections[ci]._index]._state * 

		_hidden[hi]._threshold += learnThreshold * attentions[hi] * (_hidden[hi]._state - sparsity);
	}
}

void RSDR::getVHWeights(int hx, int hy, std::vector<float> &rectangle) const {
	float hiddenToVisibleWidth = static_cast<float>(_visibleWidth) / static_cast<float>(_hiddenWidth);
	float hiddenToVisibleHeight = static_cast<float>(_visibleHeight) / static_cast<float>(_hiddenHeight);

	int dim = _receptiveRadius * 2 + 1;

	rectangle.resize(dim * dim, 0.0f);

	int hi = hx + hy * _hiddenWidth;

	int centerX = std::round(hx * hiddenToVisibleWidth);
	int centerY = std::round(hy * hiddenToVisibleHeight);

	for (int ci = 0; ci < _hidden[hi]._feedForwardConnections.size(); ci++) {
		int index = _hidden[hi]._feedForwardConnections[ci]._index;

		int vx = index % _visibleWidth;
		int vy = index / _visibleWidth;

		int dx = vx - centerX;
		int dy = vy - centerY;

		int rx = dx + _receptiveRadius;
		int ry = dy + _receptiveRadius;

		rectangle[rx + ry * dim] = _hidden[hi]._feedForwardConnections[ci]._weight;
	}
}

void RSDR::stepEnd() {
	for (int hi = 0; hi < _hidden.size(); hi++)
		_hidden[hi]._statePrev = _hidden[hi]._state;
}