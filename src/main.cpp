/*
    Universidade Federal de Minas Gerais
    Exercício Computacional 3 - EcoSim
    Aluno: Bernardo de Souza Silva
    Matrícula: 2020084290
    Disciplina: Automação em Tempo Real
*/

#define CROW_MAIN
#define CROW_STATIC_DIR "../public"

#include "crow_all.h"
#include "json.hpp"
#include <random>
#include <thread>
#include <mutex>

std::mutex plant_mtx;
std::mutex herbivore_mtx;
std::mutex carnivore_mtx;

static const uint32_t NUM_ROWS = 15;

// Constantes
const uint32_t PLANT_MAXIMUM_AGE = 10;
const uint32_t HERBIVORE_MAXIMUM_AGE = 50;
const uint32_t CARNIVORE_MAXIMUM_AGE = 80;
const uint32_t MAXIMUM_ENERGY = 200;
const uint32_t THRESHOLD_ENERGY_FOR_REPRODUCTION = 20;

// Probabilidades
const double PLANT_REPRODUCTION_PROBABILITY = 0.2;
const double HERBIVORE_REPRODUCTION_PROBABILITY = 0.075;
const double CARNIVORE_REPRODUCTION_PROBABILITY = 0.025;
const double HERBIVORE_MOVE_PROBABILITY = 0.7;
const double HERBIVORE_EAT_PROBABILITY = 0.9;
const double CARNIVORE_MOVE_PROBABILITY = 0.5;
const double CARNIVORE_EAT_PROBABILITY = 1.0;

// Definição de tipos
enum entity_type_t {
    empty,
    plant,
    herbivore,
    carnivore
};

struct pos_t {
    uint32_t i;
    uint32_t j;
};

struct entity_t {
    entity_type_t type;
    int32_t energy;
    int32_t age;
};

// Código auxilar para converter o entity_type_t enum em uma string
NLOHMANN_JSON_SERIALIZE_ENUM(entity_type_t, {
                                                {empty, " "},
                                                {plant, "P"},
                                                {herbivore, "H"},
                                                {carnivore, "C"},
                                            })

// Código auxiliar para converter o entity_t struct em um objeto JSON
namespace nlohmann {
    void to_json(nlohmann::json &j, const entity_t &e) {
        j = nlohmann::json{{"type", e.type}, {"energy", e.energy}, {"age", e.age}};
    }
}

// Grid (matriz) que contém as enidades
static std::vector<std::vector<entity_t>> entity_grid;
// Matriz que contém as posições que já foram analizadas
std::vector<std::pair<int, int>> analyzed_pos;
// Matriz que contém as posições disponíveis
std::vector<std::pair<int, int>> available_pos;

// Função para gerar um valor randômico com base na probabilidade
static std::random_device rd;
static std::mt19937 generator(rd());
bool random_action(float probability) {
    std::uniform_real_distribution<> distribution(0.0, 1.0);
    return distribution(generator) < probability;
}

// Thread da planta
void plant_thread(int i, int j) {
    plant_mtx.lock();
    // Caso tenha atingido 10 anos, a planta morre
    if (entity_grid[i][j].age == 10) {
        entity_grid[i][j] = {empty, 0, 0};
    } else {
        // Caso a planta não tenha morrido, incrementa a idade
        entity_grid[i][j].age++;
        // Lógica de reprodução da planta
        if (random_action(PLANT_REPRODUCTION_PROBABILITY)) {
            // Verifica as casas adjacentes e armazena no vetor available_pos
            if ((i + 1) < NUM_ROWS && entity_grid[i + 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i + 1, j));
            }
            if (i > 0 && entity_grid[i - 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i - 1, j));
            }
            if ((j + 1) < NUM_ROWS && entity_grid[i][j + 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j + 1));
            }
            if (j > 0 && entity_grid[i][j - 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int drawing = distribution(generator);
                int line = available_pos[drawing].first;
                int column = available_pos[drawing].second;
                entity_grid[line][column].type = plant;
                analyzed_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
    }
    plant_mtx.unlock();
}
// Thread do herbívoro
void herbivore_thread(int i, int j) {
    herbivore_mtx.lock();
    // Caso tenha atingido 50 anos, ou a energia tenha acabado, o herbívoro morre
    if (entity_grid[i][j].age == 50 || entity_grid[i][j].energy <= 0) {
        entity_grid[i][j] = {empty, 0, 0};
    } else {
        // Caso o herbívoro não tenha morrido, incrementa a idade
        entity_grid[i][j].age++;
        // Lógica de alimentação do herbívoro
        if ((i + 1) < NUM_ROWS && entity_grid[i + 1][j].type == plant && random_action(HERBIVORE_EAT_PROBABILITY)) {
            entity_grid[i + 1][j] = {empty, 0, 0};
            entity_grid[i][j].energy += 30;
        }
        if (i > 0 && entity_grid[i - 1][j].type == plant && random_action(HERBIVORE_EAT_PROBABILITY)) {
            entity_grid[i - 1][j] = {empty, 0, 0};
            entity_grid[i][j].energy += 30;
        }
        if ((j + 1) < NUM_ROWS && entity_grid[i][j + 1].type == plant && random_action(HERBIVORE_EAT_PROBABILITY)) {
            entity_grid[i][j + 1] = {empty, 0, 0};
            entity_grid[i][j].energy += 30;
        }
        if (j > 0 && entity_grid[i][j - 1].type == plant && random_action(HERBIVORE_EAT_PROBABILITY)) {
            entity_grid[i][j - 1] = {empty, 0, 0};
            entity_grid[i][j].energy += 30;
        }
        // Lógica de reprodução do herbívoro
        if (random_action(HERBIVORE_REPRODUCTION_PROBABILITY) && entity_grid[i][j].energy >= 20) {
            // Verifica as casas adjacentes e armazena no vetor available_pos
            if ((i + 1) < NUM_ROWS && entity_grid[i + 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i + 1, j));
            }
            if (i > 0 && entity_grid[i - 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i - 1, j));
            }
            if ((j + 1) < NUM_ROWS && entity_grid[i][j + 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j + 1));
            }
            if (j > 0 && entity_grid[i][j - 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int drawing = distribution(generator);
                int line = available_pos[drawing].first;
                int column = available_pos[drawing].second;
                entity_grid[line][column].type = herbivore;
                entity_grid[line][column].energy = 100;
                entity_grid[i][j].energy = entity_grid[i][j].energy - 10;
                analyzed_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
        // Lógica de movimentação do herbívoro
        if (random_action(HERBIVORE_MOVE_PROBABILITY)) {
            // Verifica as casas adjacentes e armazena no vetor available_pos
            if ((i + 1) < NUM_ROWS && entity_grid[i + 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i + 1, j));
            }
            if (i > 0 && entity_grid[i - 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i - 1, j));
            }
            if ((j + 1) < NUM_ROWS && entity_grid[i][j + 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j + 1));
            }
            if (j > 0 && entity_grid[i][j - 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int drawing = distribution(generator);
                int line = available_pos[drawing].first;
                int column = available_pos[drawing].second;
                entity_grid[line][column] = {herbivore, entity_grid[i][j].energy - 5, entity_grid[i][j].age};
                entity_grid[i][j] = {empty, 0, 0};
                analyzed_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
    }
    herbivore_mtx.unlock();
}
// Thread do carnívoro
void carnivore_thread(int i, int j) {
    carnivore_mtx.lock();
    // Caso tenha atingido 100 anos, ou a energia tenha acabado, o carnívoro morre
    if (entity_grid[i][j].age == 100 || entity_grid[i][j].energy <= 0) {
        entity_grid[i][j] = {empty, 0, 0};
    } else {
        // Caso o carnívoro não tenha morrido, incrementa a idade
        entity_grid[i][j].age++;
        // Lógica de alimentação do carnívoro
        if (i + 1 < NUM_ROWS && entity_grid[i + 1][j].type == herbivore && random_action(CARNIVORE_EAT_PROBABILITY)) {
            entity_grid[i + 1][j] = {empty, 0, 0};
            entity_grid[i][j].energy += 20;
        }
        if (i > 0 && entity_grid[i - 1][j].type == herbivore && random_action(CARNIVORE_EAT_PROBABILITY)) {
            entity_grid[i - 1][j] = {empty, 0, 0};
            entity_grid[i][j].energy += 20;
        }
        if (j + 1 < NUM_ROWS && entity_grid[i][j + 1].type == herbivore && random_action(CARNIVORE_EAT_PROBABILITY)) {
            entity_grid[i][j + 1] = {empty, 0, 0};
            entity_grid[i][j].energy += 20;
        }
        if (j > 0 && entity_grid[i][j - 1].type == herbivore && random_action(CARNIVORE_EAT_PROBABILITY)) {
            entity_grid[i][j - 1] = {empty, 0, 0};
            entity_grid[i][j].energy += 20;
        }
        // Lógica de reprodução do carnívoro
        if (random_action(CARNIVORE_REPRODUCTION_PROBABILITY) && entity_grid[i][j].energy >= 20) {
            // Verifica as casas adjacentes e armazena no vetor available_pos
            if ((i + 1) < NUM_ROWS && entity_grid[i + 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i + 1, j));
            }
            if (i > 0 && entity_grid[i - 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i - 1, j));
            }
            if ((j + 1) < NUM_ROWS && entity_grid[i][j + 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j + 1));
            }
            if (j > 0 && entity_grid[i][j - 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int drawing = distribution(generator);
                int line = available_pos[drawing].first;
                int column = available_pos[drawing].second;
                entity_grid[line][column] = {carnivore, 100, 0};
                entity_grid[i][j].energy = entity_grid[i][j].energy - 10;
                analyzed_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
        // Lógica de movimentação do carnívoro
        if (random_action(CARNIVORE_MOVE_PROBABILITY)) {
            // Verifica as casas adjacentes e armazena no vetor available_pos
            if ((i + 1) < NUM_ROWS && entity_grid[i + 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i + 1, j));
            }
            if (i > 0 && entity_grid[i - 1][j].type == empty) {
                available_pos.push_back(std::make_pair(i - 1, j));
            }
            if ((j + 1) < NUM_ROWS && entity_grid[i][j + 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j + 1));
            }
            if (j > 0 && entity_grid[i][j - 1].type == empty) {
                available_pos.push_back(std::make_pair(i, j - 1));
            }
            if (!available_pos.empty()) {
                std::uniform_int_distribution<> distribution(0, available_pos.size() - 1);
                int drawing = distribution(generator);
                int line = available_pos[drawing].first;
                int column = available_pos[drawing].second;
                entity_grid[line][column] = {carnivore, entity_grid[i][j].energy - 5, entity_grid[i][j].age};
                entity_grid[i][j] = {empty, 0, 0};
                analyzed_pos.push_back(std::make_pair(line, column));
                available_pos.clear();
            }
        }
    }
    carnivore_mtx.unlock();
}

int main() {
    crow::SimpleApp app;

    // Endpoint que serve a página HTML
    CROW_ROUTE(app, "/")
    ([](crow::request &, crow::response &res) {
        // Retorna o conteúdo HTML
        res.set_static_file_info_unsafe("../public/index.html");
        res.end(); 
    });

    // Endpoint que inicia a simulação, com os parâmetros estabelecidos
    CROW_ROUTE(app, "/start-simulation").methods("POST"_method)([](crow::request &req, crow::response &res) {
        // Faz o parse no body do JSON
        nlohmann::json request_body = nlohmann::json::parse(req.body);
        // Valida o número total de entidades no body
        uint32_t total_entities = (uint32_t)request_body["plants"] + (uint32_t)request_body["herbivores"] + (uint32_t)request_body["carnivores"];
        if (total_entities > NUM_ROWS * NUM_ROWS) {
            res.code = 400;
            res.body = "Too many entities";
            res.end();
            return;
        }
        // Limpa o grid de entidades
        entity_grid.clear();
        entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_ROWS, { empty, 0, 0 }));
        std::uniform_int_distribution<> distribution(0, 14);
        // Função para criar uma entidade em uma posição aleatória
        auto create_entity = [&](entity_type_t type, int energy) {
            int line, column;
            do {
                line = distribution(generator);
                column = distribution(generator);
            } while (entity_grid[line][column].type != empty);
            entity_grid[line][column] = {type, energy, 0};
        };
        // Criação das plantas
        for (uint32_t i = 0; i < (uint32_t)request_body["plants"]; i++) {
            create_entity(plant, 0);
        }
        // Criação dos herbívoros
        for (uint32_t i = 0; i < (uint32_t)request_body["herbivores"]; i++) {
            create_entity(herbivore, 100);
        }
        // Criação dos carnívoros
        for (uint32_t i = 0; i < (uint32_t)request_body["carnivores"]; i++) {
            create_entity(carnivore, 100);
        }
        // Retorna o JSON que representa o grid de entidades
        nlohmann::json json_grid = entity_grid;
        res.body = json_grid.dump();
        res.end();
    });

    // Endpoint para avançar a simulação para a próxima iteração
    CROW_ROUTE(app, "/next-iteration").methods("GET"_method)([]() {
        bool analyzed = false;
        for (uint32_t i = 0; i < NUM_ROWS; i++) {
            for (uint32_t j = 0; j < NUM_ROWS; j++) {
                // Verifica se a posição no grid já foi analisada
                analyzed = false;
                for (int k = 0; k < analyzed_pos.size(); k++) {
                    if (analyzed_pos[k].first == i && analyzed_pos[k].second == j) {
                        analyzed = true;
                    }
                }
                // Caso a posição no grid não tenha sido analisada
                if (!analyzed) {
                    // Caso a entidade seja do tipo planta
                    if (entity_grid[i][j].type == plant) {
                        std::thread tplant(plant_thread, i, j);
                        tplant.join();
                    } 
                    // Caso a entidade seja do tipo herbívoro
                    else if (entity_grid[i][j].type == herbivore) {
                        std::thread therbivore(herbivore_thread, i, j);
                        therbivore.join();
                    } 
                    // Caso a entidade seja do tipo carnívoro
                    else if (entity_grid[i][j].type == carnivore) {
                        std::thread tcarnivore(carnivore_thread, i, j);
                        tcarnivore.join();
                    }
                }
            }
        }
        analyzed_pos.clear();
        // Retorna a representação do grid em JSON
        nlohmann::json json_grid = entity_grid; 
        return json_grid.dump(); 
    });
    // Roda o servidor
    app.port(8080).run();
    return 0;
}