#pragma once

#include "RecurrentSparseCoder2D.h"

namespace sc {
	class HTSL {
	public:
		struct LayerDesc {
			int _width, _height;

			int _receptiveRadius;
			int _inhibitionRadius;
			int _recurrentRadius;

			int _feedbackRadius;
			int _lateralRadius;

			float _sparsity;

			float _rscAlpha;
			float _rscBeta;
			float _rscGamma;
			float _rscDelta;

			float _predictionAlpha;

			LayerDesc()
				: _width(16), _height(16),
				_receptiveRadius(4), _inhibitionRadius(4), _recurrentRadius(4),
				_feedbackRadius(4), _lateralRadius(4),
				_sparsity(0.02f), 
				_rscAlpha(0.05f), _rscBeta(0.001f), _rscGamma(0.05f), _rscDelta(10.0f),
				_predictionAlpha(0.4f)
			{}
		};

		static float sigmoid(float x) {
			return 1.0f / (1.0f + std::exp(-x));
		}

	private:
		struct PredictionConnection {
			float _weight;
			float _falloff;

			unsigned short _index;
		};

		struct PredictionNode {
			std::vector<PredictionConnection> _feedbackConnections;
			std::vector<PredictionConnection> _lateralConnections;

			float _state;
			float _statePrev;

			PredictionNode()
				: _state(0.0f), _statePrev(0.0f)
			{}
		};

		struct Layer {
			RecurrentSparseCoder2D _rsc;

			std::vector<PredictionNode> _predictionNodes;
		};

		std::vector<LayerDesc> _layerDescs;
		std::vector<Layer> _layers;

	public:
		void createRandom(int inputWidth, int inputHeight, const std::vector<LayerDesc> &layerDescs, std::mt19937 &generator);

		void setInput(int index, float value) {
			_layers.front()._rsc.setVisibleInput(index, value);
		}

		void setInput(int x, int y, float value) {
			_layers.front()._rsc.setVisibleInput(x, y, value);
		}

		float getPrediction(int index) const {
			return _layers.front()._predictionNodes[index]._state;
		}

		float getPrediction(int x, int y) const {
			return _layers.front()._predictionNodes[x + y * _layerDescs.front()._width]._state;
		}

		void update();
		void learnRSC();
		void learnPrediction();
		void stepEnd();

		const std::vector<LayerDesc> &getLayerDescs() const {
			return _layerDescs;
		}

		const std::vector<Layer> &getLayers() const {
			return _layers;
		}
	};
}