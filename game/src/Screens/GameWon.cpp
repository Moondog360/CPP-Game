#include <Shaders.hpp>
#include <Model_Sprite.hpp>
#include "Game.hpp"
#include "Screens/GameWon.hpp"

GameWon::GameWon()
{
    _sound = ServiceLocator::getAudio();
	_sound->setSoundLevel(Game::_settings.volume * 20);
	if (Game::_settings.sound)
		_sound->playSound("../../Assets/sounds/gameplay/winner.wav");
    this->_model = new Model_Sprite("../../Assets/buttons/winner.obj");
    this->_shader = new Shaders("../assets/shaders/vert/MeshVert.glsl","../assets/shaders/frag/MeshFrag.glsl");
    this->_model->Position(Game::_window.Width() >> 1, -40.0f, Game::_window.Height() >> 1);
    this->_model->Scale(7);
}

GameWon::GameWon(GameWon const & src)
{
    *this= src;
}

GameWon::~GameWon()
{
    delete this->_model;
    delete this->_shader;
}

void GameWon::show()
{
    float startTime = glfwGetTime();
    glm::mat4 projection = Game::_window.Projection();
    float currentTime = glfwGetTime();
    while (currentTime - startTime < 2.0f)
    {
        Game::_window.clear();
        this->_shader->use();
        this->_shader->setMat4("projection", projection);
        this->_shader->setMat4("view", glm::mat4());
        this->_model->DrawScaledBy(*this->_shader, 1.0f + (currentTime - startTime) / 4.0f);
        Game::_window.update();
        currentTime = glfwGetTime();
    }
}

GameWon & GameWon::operator=(GameWon const & src)
{
	if (this != &src)
		*this = src;
	return (*this);
}