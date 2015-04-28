#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "sc/SparseCoder.h"

int main() {
	std::mt19937 generator(time(nullptr));

	// ---------------------------- Simulation Parameters ----------------------------

	const int sampleWidth = 16;
	const int sampleHeight = 16;
	const int codeWidth = 16;
	const int codeHeight = 16;
	const float learnAlpha = 0.005f;
	const float learnBeta = 0.002f;
	const float learnGamma = 0.005f;
	const float sparsity = 0.02f;

	const int stepsPerFrame = 4;

	// --------------------------- Create the Sparse Coder ---------------------------

	sc::SparseCoder sparseCoder;

	sparseCoder.createRandom(sampleWidth * sampleHeight, codeWidth * codeHeight, generator);

	// ------------------------------- Load Resources --------------------------------

	sf::Image sampleImage;

	sampleImage.loadFromFile("testImage.png");

	sf::Texture sampleTexture;

	sampleTexture.loadFromImage(sampleImage);

	sf::Image reconstructionImage;
	reconstructionImage.loadFromFile("resources/noreconstruction.png");

	sf::Texture reconstructionTexture;
	reconstructionTexture.loadFromImage(reconstructionImage);

	// ------------------------------- Simulation Loop -------------------------------

	sf::RenderWindow renderWindow;

	renderWindow.create(sf::VideoMode(1280, 720), "Sparse Coding", sf::Style::Default);

	renderWindow.setVerticalSyncEnabled(true);
	renderWindow.setFramerateLimit(60);

	std::uniform_int_distribution<int> widthDist(0, static_cast<int>(sampleImage.getSize().x) - sampleWidth - 1);
	std::uniform_int_distribution<int> heightDist(0, static_cast<int>(sampleImage.getSize().y) - sampleHeight - 1);

	bool quit = false;

	do {
		sf::Event event;

		while (renderWindow.pollEvent(event)) {
			switch (event.type) {
			case sf::Event::Closed:
				quit = true;
				break;
			}
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
			quit = true;

		renderWindow.clear();

		// -------------------------- Sparse Coding ----------------------------

		for (int s = 0; s < stepsPerFrame; s++) {
			int sampleX = widthDist(generator);
			int sampleY = heightDist(generator);

			int inputIndex = 0;

			for (int x = 0; x < sampleWidth; x++)
				for (int y = 0; y < sampleHeight; y++) {
					int tx = sampleX + x;
					int ty = sampleY + y;

					sparseCoder.setVisibleInput(inputIndex++, sampleImage.getPixel(tx, ty).r / 255.0f);
				}

			sparseCoder.activate();

			sparseCoder.reconstruct();

			sparseCoder.learn(learnAlpha, learnBeta, learnGamma, sparsity);
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::R)) {
			// Perform reconstruction
			std::vector<float> recon(sampleImage.getSize().x * sampleImage.getSize().y, 0.0f);
			std::vector<float> sums(sampleImage.getSize().x * sampleImage.getSize().y, 0.0f);

			for (int wx = 0; wx < sampleImage.getSize().x - sampleWidth; wx++)
				for (int wy = 0; wy < sampleImage.getSize().y - sampleHeight; wy++) {
					int inputIndex = 0;

					for (int x = 0; x < sampleWidth; x++)
						for (int y = 0; y < sampleHeight; y++) {
							int tx = wx + x;
							int ty = wy + y;

							sparseCoder.setVisibleInput(inputIndex++, sampleImage.getPixel(tx, ty).r / 255.0f);
						}

					sparseCoder.activate();

					sparseCoder.reconstruct();

					inputIndex = 0;

					for (int x = 0; x < sampleWidth; x++)
						for (int y = 0; y < sampleHeight; y++) {
							int tx = wx + x;
							int ty = wy + y;

							recon[tx + ty * sampleImage.getSize().x] += sparseCoder.getVisibleRecon(inputIndex++);
							sums[tx + ty * sampleImage.getSize().x] += 1.0f;
						}
				}

			float minimum = 99999.0f;
			float maximum = -99999.0f;

			for (int x = 0; x < sampleImage.getSize().x; x++)
				for (int y = 0; y < sampleImage.getSize().y; y++) {
					recon[x + y * sampleImage.getSize().x] /= sums[x + y * sampleImage.getSize().x];

					minimum = std::min(minimum, recon[x + y * sampleImage.getSize().x]);
					maximum = std::max(maximum, recon[x + y * sampleImage.getSize().x]);
				}

			reconstructionImage.create(sampleImage.getSize().x, sampleImage.getSize().y);

			for (int x = 0; x < sampleImage.getSize().x; x++)
				for (int y = 0; y < sampleImage.getSize().y; y++) {
					sf::Color c;

					c.r = c.g = c.b = 255.0f * (recon[x + y * sampleImage.getSize().x] - minimum) / (maximum - minimum);
					c.a = 255;

					reconstructionImage.setPixel(x, y, c);
				}

			reconstructionTexture.loadFromImage(reconstructionImage);
		}

		// ----------------------------- Rendering -----------------------------

		float minWeight = 9999.0f;
		float maxWeight = -9999.0f;

		for (int sx = 0; sx < codeWidth; sx++)
			for (int sy = 0; sy < codeHeight; sy++) {
				for (int x = 0; x < sampleWidth; x++)
					for (int y = 0; y < sampleHeight; y++) {
						float w = sparseCoder.getVHWeight(sx + sy * codeWidth, x + y * sampleWidth);

						minWeight = std::min(minWeight, w);
						maxWeight = std::max(maxWeight, w);
					}
			}

		sf::Image receptiveFieldsImage;
		receptiveFieldsImage.create(codeWidth * sampleWidth, codeHeight * sampleHeight);

		float scalar = 1.0f / (maxWeight - minWeight);

		for (int sx = 0; sx < codeWidth; sx++)
			for (int sy = 0; sy < codeHeight; sy++) {
				for (int x = 0; x < sampleWidth; x++)
					for (int y = 0; y < sampleHeight; y++) {
						sf::Color color;

						color.r = color.b = color.g = 255 * scalar * (sparseCoder.getVHWeight(sx + sy * codeWidth, x + y * sampleWidth) - minWeight);
						color.a = 255;

						receptiveFieldsImage.setPixel(sx * sampleWidth + x, sy * sampleHeight + y, color);
					}
			}

		sf::Texture receptiveFieldsTexture;
		receptiveFieldsTexture.loadFromImage(receptiveFieldsImage);

		sf::Sprite receptiveFieldsSprite;
		receptiveFieldsSprite.setTexture(receptiveFieldsTexture);

		float scale = static_cast<float>(renderWindow.getSize().y) / static_cast<float>(receptiveFieldsImage.getSize().y);

		receptiveFieldsSprite.setScale(sf::Vector2f(scale, scale));

		renderWindow.draw(receptiveFieldsSprite);

		for (int sx = 0; sx < codeWidth; sx++)
			for (int sy = 0; sy < codeHeight; sy++) {
				if (sparseCoder.getHiddenState(sx + sy * codeWidth) > 0.0f) {
					sf::RectangleShape rs;

					rs.setPosition(sx * sampleWidth * scale, sy * sampleHeight * scale);
					rs.setOutlineColor(sf::Color::Red);
					rs.setFillColor(sf::Color::Transparent);
					rs.setOutlineThickness(2.0f);

					rs.setSize(sf::Vector2f(sampleWidth * scale, sampleHeight * scale));

					renderWindow.draw(rs);
				}
			}

		sf::Sprite sampleSprite;
		sampleSprite.setTexture(sampleTexture);

		sampleSprite.setPosition(sf::Vector2f(renderWindow.getSize().x - sampleImage.getSize().x, 0.0f));
		
		renderWindow.draw(sampleSprite);

		sf::Sprite reconstructionSprite;
		reconstructionSprite.setTexture(reconstructionTexture);

		reconstructionSprite.setPosition(sf::Vector2f(renderWindow.getSize().x - reconstructionImage.getSize().x, renderWindow.getSize().y - reconstructionImage.getSize().y));

		renderWindow.draw(reconstructionSprite);

		renderWindow.display();
	} while (!quit);
	
	return 0;
}