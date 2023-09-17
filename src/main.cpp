#define CROW_MAIN
#define CROW_STATIC_DIR "../public"

#include "crow_all.h"
#include "json.hpp"
#include <iostream>
#include <random>

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

std::vector<std::pair<int, int>> analyzed_pairs;
std::vector<std::pair<int, int>> available_pos;

// Função para gerar um valor randômico com base na probabilidade
static std::random_device rd;
static std::mt19937 gen(rd());
bool random_action(float probability) {
    std::uniform_real_distribution<> dis(0.0, 1.0);
    return dis(gen) < probability;
}

int main()
{
    crow::SimpleApp app;

    // Endpoint que serve a página HTML
    CROW_ROUTE(app, "/")([](crow::request &, crow::response &res) {
        // Retorna o conteúdo HTML
        res.set_static_file_info_unsafe("../public/index.html");
        res.end(); 
    });

    // Endpoint que inicia a simulação, com os parâmetros iniciais
    CROW_ROUTE(app, "/start-simulation").methods("POST"_method)([](crow::request &req, crow::response &res) {
        // Faz o parse no body do JSON
        nlohmann::json request_body = nlohmann::json::parse(req.body);

        // Valida o body do JSON
        uint32_t total_entities = (uint32_t)request_body["plants"] + (uint32_t)request_body["herbivores"] + (uint32_t)request_body["carnivores"];
        if (total_entities > NUM_ROWS * NUM_ROWS) {
            res.code = 400;
            res.body = "Too many entities";
            res.end();
            return;
        }

        // Limpa o grid de entidades
        entity_grid.clear();
        entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_ROWS, { empty, 0, 0}));
        std::uniform_int_distribution<> dis(0, 14);
        int line = dis(gen);
        int column = dis(gen);

        // Criação das plantas
        for (uint32_t i = 0; i < (uint32_t) request_body["plants"]; i++) {
            while (!entity_grid[line][column].type == empty) {
                line = dis(gen);
                column = dis(gen); 
            }
            entity_grid[line][column].type = plant;
            entity_grid[line][column].age = 0;
        }

        // Criação dos herbívoros
        for (uint32_t i = 0; i < (uint32_t) request_body["herbivores"]; i++) {
            while (!entity_grid[line][column].type == empty) {
                line = dis(gen);
                column = dis(gen); 
            }
            entity_grid[line][column].type = herbivore;
            entity_grid[line][column].age = 0;
            entity_grid[line][column].energy = 100;
        }

        // Criação dos carnívoros
        for (uint32_t i = 0; i < (uint32_t) request_body["carnivores"]; i++) {
            while (!entity_grid[line][column].type == empty) {
                line = dis(gen);
                column = dis(gen); 
            }
            entity_grid[line][column].type = carnivore;
            entity_grid[line][column].age = 0;
            entity_grid[line][column].energy = 100;
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
                for (int k = 0; k < analyzed_pairs.size(); k++) {
                    if (analyzed_pairs[k].first == i && analyzed_pairs[k].second == j){
                        analyzed = true;
                    }
                }

                // Caso não a posição no grid não tenha sido analisada
                if (!analyzed){
                    // Caso a entidade seja do tipo planta
                    if (entity_grid[i][j].type == plant){
                        // Caso tenha atingido 10 anos, a planta morre
                        if (entity_grid[i][j].age == 10) {
                            entity_grid[i][j].type = empty;
                            entity_grid[i][j].energy = 0;
                            entity_grid[i][j].age = 0;
                        } else {
                            // Caso a planta não tenha morrido, incrementa a idade
                            entity_grid[i][j].age++;
                            // Lógica de reprodução da planta
                            if (random_action(PLANT_REPRODUCTION_PROBABILITY)) {
                                // Verifica as casas adjacentes e armazena no vetor available_pos 
                                if ((i + 1) < NUM_ROWS){
                                    if (entity_grid[i + 1][j].type == empty){
                                        available_pos.push_back(std::make_pair(i + 1, j));
                                    }
                                }

                                if (i > 0) {
                                    if (entity_grid[i - 1][j].type == empty){
                                        available_pos.push_back(std::make_pair(i - 1, j));
                                    }
                                }

                                if ((j + 1) < NUM_ROWS){
                                    if (entity_grid[i][j + 1].type == empty){
                                        available_pos.push_back(std::make_pair(i, j + 1));
                                    }
                                }

                                if (j > 0) {
                                    if (entity_grid[i][j - 1].type == empty){
                                        available_pos.push_back(std::make_pair(i, j - 1));
                                    }
                                }

                                if (!available_pos.empty()) {
                                    std::uniform_int_distribution<> dis(0, available_pos.size()-1);
                                    int drawing = dis(gen);
                                    int line = available_pos[drawing].first;
                                    int column = available_pos[drawing].second;
                                    entity_grid[line][column].type = plant;
                                    analyzed_pairs.push_back(std::make_pair(line, column));
                                    available_pos.clear();
                                }
                            }
                        }
                    } 
                    // Caso a entidade seja do tipo herbívoro
                    else if (entity_grid[i][j].type == herbivore) {
                        // Caso tenha atingido 50 anos, ou a energia tenha acabado, o herbívoro morre
                        if (entity_grid[i][j].age == 50 || entity_grid[i][j].energy <= 0) {
                            entity_grid[i][j].type = empty;
                            entity_grid[i][j].energy = 0;
                            entity_grid[i][j].age = 0;
                        } else {
                            // Caso o herbívoro não tenha morrido, incrementa a idade
                            entity_grid[i][j].age++;
                            // Lógica de alimentação do herbívoro
                            if((i + 1) < NUM_ROWS) {
                                if (entity_grid[i + 1][j].type == plant) {
                                    if (random_action(HERBIVORE_EAT_PROBABILITY)){
                                        entity_grid[i+1][j].type = empty;
                                        entity_grid[i+1][j].age = 0;
                                        entity_grid[i+1][j].energy = 0;
                                        entity_grid[i][j].energy = entity_grid[i][j].energy + 30;
                                    }
                                }
                            }

                            if (i > 0) {
                                if (entity_grid[i - 1][j].type == plant) {
                                    if (random_action(HERBIVORE_EAT_PROBABILITY)) {
                                        entity_grid[i - 1][j].type = empty;
                                        entity_grid[i - 1][j].age = 0;
                                        entity_grid[i - 1][j].energy = 0;
                                        entity_grid[i][j].energy = entity_grid[i][j].energy + 30;
                                    }
                                }
                            }

                            if ((j + 1) < NUM_ROWS){
                                if (entity_grid[i][j + 1].type == plant){
                                    if (random_action(HERBIVORE_EAT_PROBABILITY)) {
                                        entity_grid[i][j + 1].type = empty;
                                        entity_grid[i][j + 1].age = 0;
                                        entity_grid[i][j + 1].energy = 0;
                                        entity_grid[i][j].energy = entity_grid[i][j].energy + 30;
                                    }
                                }
                            }

                            if (j > 0){
                                if (entity_grid[i][j - 1].type == plant){
                                    if (random_action(HERBIVORE_EAT_PROBABILITY)) {
                                        entity_grid[i][j-1].type = empty;
                                        entity_grid[i][j-1].age = 0;
                                        entity_grid[i][j-1].energy = 0;
                                        entity_grid[i][j].energy = entity_grid[i][j].energy + 30;
                                    }
                                }
                            }

                            // Lógica de reprodução do herbívoro
                            if (random_action(HERBIVORE_REPRODUCTION_PROBABILITY) && entity_grid[i][j].energy >= 20) {
                                // Verifica as casas adjacentes e armazena no vetor available_pos 
                                if ((i + 1) < NUM_ROWS){
                                    if (entity_grid[i + 1][j].type == empty) {
                                        available_pos.push_back(std::make_pair(i + 1, j));
                                    }
                                }

                                if (i > 0) {
                                    if (entity_grid[i - 1][j].type == empty) {
                                        available_pos.push_back(std::make_pair(i - 1, j));
                                    }
                                }

                                if ((j + 1) < NUM_ROWS) {
                                    if (entity_grid[i][j + 1].type == empty) {
                                        available_pos.push_back(std::make_pair(i, j + 1));
                                    }
                                }
                                if (j > 0) {
                                    if (entity_grid[i][j - 1].type == empty) {
                                        available_pos.push_back(std::make_pair(i, j - 1));
                                    }
                                }
                                
                                if (!available_pos.empty()) {
                                    std::uniform_int_distribution<> dis(0, available_pos.size() - 1);
                                    int drawing = dis(gen);
                                    int line = available_pos[drawing].first;
                                    int column = available_pos[drawing].second;
                                    entity_grid[line][column].type = herbivore;
                                    entity_grid[line][column].energy = 100;
                                    entity_grid[i][j].energy = entity_grid[i][j].energy - 10;
                                    analyzed_pairs.push_back(std::make_pair(line, column));
                                    available_pos.clear();
                                }
                            }

                            // Lógica de movimentação do herbívoro
                            if (random_action(HERBIVORE_MOVE_PROBABILITY)) {
                                // Verifica as casas adjacentes e armazena no vetor available_pos 
                                if ((i + 1) < NUM_ROWS) {
                                    if (entity_grid[i + 1][j].type == empty) {
                                        available_pos.push_back(std::make_pair(i + 1, j));
                                    }
                                }

                                if (i > 0){
                                    if (entity_grid[i - 1][j].type == empty) {
                                        available_pos.push_back(std::make_pair(i - 1, j));
                                    }
                                }

                                if ((j + 1) < NUM_ROWS){
                                    if (entity_grid[i][j + 1].type == empty) {
                                        available_pos.push_back(std::make_pair(i, j + 1));
                                    }
                                }

                                if (j > 0) {
                                    if (entity_grid[i][j - 1].type == empty) {
                                        available_pos.push_back(std::make_pair(i, j - 1));
                                    }
                                }

                                if (!available_pos.empty()) {
                                    std::uniform_int_distribution<> dis(0, available_pos.size() - 1);
                                    int drawing = dis(gen);
                                    int line = available_pos[drawing].first;
                                    int column = available_pos[drawing].second;
                                    entity_grid[line][column].type = herbivore;
                                    entity_grid[line][column].age = entity_grid[i][j].age;
                                    entity_grid[line][column].energy = entity_grid[i][j].energy - 5;
                                    entity_grid[i][j].type = empty;
                                    entity_grid[i][j].age = 0;
                                    entity_grid[i][j].energy = 0;
                                    analyzed_pairs.push_back(std::make_pair(line, column));
                                    available_pos.clear();
                                }
                            }
                        }
                    } 
                    // Caso a entidade seja do tipo carnívoro
                    else if (entity_grid[i][j].type == carnivore) {
                        // Caso tenha atingido 100 anos, ou a energia tenha acabado, o carnívoro morre
                        if (entity_grid[i][j].age == 100 || entity_grid[i][j].energy <= 0) {
                            entity_grid[i][j].type = empty;
                            entity_grid[i][j].energy = 0;
                            entity_grid[i][j].age = 0;
                        } else {
                            // Caso o carnívoro não tenha morrido, incrementa a idade
                            entity_grid[i][j].age++;
                            // Lógica de alimentação do carnívoro
                            if ((i + 1) < NUM_ROWS) {
                                if (entity_grid[i + 1][j].type == herbivore) {
                                    if(random_action(CARNIVORE_EAT_PROBABILITY)) {
                                        entity_grid[i + 1][j].type = empty;
                                        entity_grid[i + 1][j].age = 0;
                                        entity_grid[i + 1][j].energy = 0;
                                        entity_grid[i][j].energy = entity_grid[i][j].energy + 20;
                                    }
                                }
                            }

                            if (i > 0) {
                                if (entity_grid[i - 1][j].type == herbivore) {
                                    if(random_action(CARNIVORE_EAT_PROBABILITY)) {
                                        entity_grid[i - 1][j].type = empty;
                                        entity_grid[i - 1][j].age = 0;
                                        entity_grid[i - 1][j].energy = 0;
                                        entity_grid[i][j].energy = entity_grid[i][j].energy + 20;
                                    }
                                }
                            }

                            if ((j + 1) < NUM_ROWS) {
                                if (entity_grid[i][j + 1].type == herbivore) {
                                    if(random_action(CARNIVORE_EAT_PROBABILITY)){
                                        entity_grid[i][j + 1].type = empty;
                                        entity_grid[i][j + 1].age = 0;
                                        entity_grid[i][j + 1].energy = 0;
                                        entity_grid[i][j].energy = entity_grid[i][j].energy + 20;
                                    }
                                }
                            }

                            if (j > 0) {
                                if (entity_grid[i][j - 1].type == herbivore) {
                                    if(random_action(CARNIVORE_EAT_PROBABILITY)) {
                                        entity_grid[i][j - 1].type = empty;
                                        entity_grid[i][j - 1].age = 0;
                                        entity_grid[i][j - 1].energy = 0;
                                        entity_grid[i][j].energy = entity_grid[i][j].energy + 20;
                                    }
                                }
                            }

                            // Lógica de reprodução do carnívoro
                            if (random_action(CARNIVORE_REPRODUCTION_PROBABILITY) && entity_grid[i][j].energy >= 20) {
                                // Verifica as casas adjacentes e armazena no vetor available_pos
                                if ((i + 1) < NUM_ROWS) {
                                    if (entity_grid[i + 1][j].type == empty) {
                                        available_pos.push_back(std::make_pair(i + 1, j));
                                    }
                                }

                                if (i > 0) {
                                    if (entity_grid[i - 1][j].type == empty) {
                                        available_pos.push_back(std::make_pair(i - 1, j));
                                    }
                                }

                                if ((j + 1) < NUM_ROWS){
                                    if (entity_grid[i][j + 1].type == empty) {
                                        available_pos.push_back(std::make_pair(i, j + 1));
                                    }
                                }

                                if (j > 0) {
                                    if (entity_grid[i][j - 1].type == empty){
                                        available_pos.push_back(std::make_pair(i, j - 1));
                                    }
                                }

                                if (!available_pos.empty()) {
                                    std::uniform_int_distribution<> dis(0, available_pos.size() - 1);
                                    int drawing = dis(gen);
                                    int line = available_pos[drawing].first;
                                    int column = available_pos[drawing].second;
                                    entity_grid[line][column].type = carnivore;
                                    entity_grid[line][column].energy = 100;
                                    entity_grid[line][column].age = 0;
                                    entity_grid[i][j].energy = entity_grid[i][j].energy - 10;
                                    analyzed_pairs.push_back(std::make_pair(line, column));
                                    available_pos.clear();
                                }
                            }

                            // Lógica de movimentação do carnívoro
                            if (random_action(CARNIVORE_MOVE_PROBABILITY)) {
                                // Verifica as casas adjacentes e armazena no vetor available_pos
                                if ((i + 1) < NUM_ROWS) {
                                    if (entity_grid[i + 1][j].type == empty) {
                                        available_pos.push_back(std::make_pair(i + 1, j));
                                    }
                                }

                                if (i > 0) {
                                    if (entity_grid[i - 1][j].type == empty) {
                                        available_pos.push_back(std::make_pair(i - 1, j));
                                    }
                                }

                                if ((j + 1) < NUM_ROWS) {
                                    if (entity_grid[i][j + 1].type == empty) {
                                        available_pos.push_back(std::make_pair(i, j + 1));
                                    }
                                }

                                if (j > 0) {
                                    if (entity_grid[i][j - 1].type == empty) {
                                        available_pos.push_back(std::make_pair(i, j - 1));
                                    }
                                }

                                if (!available_pos.empty()) {
                                    std::uniform_int_distribution<> dis(0, available_pos.size() - 1);
                                    int drawing = dis(gen);
                                    int line = available_pos[drawing].first;
                                    int column = available_pos[drawing].second;
                                    entity_grid[line][column].type = carnivore;
                                    entity_grid[line][column].age = entity_grid[i][j].age;
                                    entity_grid[line][column].energy = entity_grid[i][j].energy - 5; 
                                    entity_grid[i][j].type = empty;
                                    entity_grid[i][j].age = 0;
                                    entity_grid[i][j].energy = 0;
                                    analyzed_pairs.push_back(std::make_pair(line, column));
                                    available_pos.clear();
                                }
                            }
                        }

                     }
                }
            }
        }

        analyzed_pairs.clear();
        // Retorna a representação do grid em JSON
        nlohmann::json json_grid = entity_grid; 
        return json_grid.dump(); 
    });

    app.port(8080).run();

    return 0;
}