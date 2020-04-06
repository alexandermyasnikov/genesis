
# GENESIS

* Author: Alexander Myasnikov
* mailto:myasnikov.alexander.s@gmail.com
* git:https://gitlab.com/amyasnikov/genesis
* site:http://amyasnikov.pro



### Цель

* Моделирование возникновения жизни.



### Demo

![Demo v0.4](/resources/demo_v0.4.gif)



### World

* Мир - прямоугольная область, состоящая из клеток.
* В мире генерируются разные ресурсы, необходимые ботам для жизни.
* В одной клетке может находиться один бот.



### Bot (Microbe)

* Бот содержит геном, который определяет его поведение.
* Бот может хранить в себе ресурсы и обмениваться ими с окружающей средой.
* Бот может преобразовывать одни ресурсы в другие.
* Бот может атаковать других ботов.
* Время жизни бота ограничено.



### TODO

* Добавить возможность реагировать бота на раздражения (прерывания).
* Убрать msgpack



### Config

* Все значимые параметры вынесены в конфигурационный файл.
* Пример файла:

```
{
  "age_max": 1000,
  "age_max_delta": 1,
  "binary_data": false,
  "code_size": 64,
  "debug": [
    "_mind",
    "_trace",
    "debug",
    "error"
  ],
  "energy_remaining": 3,
  "interval_save_world_ms": 1800000,
  "interval_update_world_ms": 1,
  "mutation_probability": 0.1,
  "recipe_clone": "clone",
  "recipe_init": "init",
  "recipe_step": "step",
  "recipes": [
    {
      "available": false,
      "in_out": [
        [
          "energy",
          100
        ],
        [
          "organic",
          100
        ]
      ],
      "name": "init"
    },
    {
      "available": false,
      "in_out": [
        [
          "energy",
          -1
        ]
      ],
      "name": "step"
    },
    {
      "available": false,
      "in_out": [
        [
          "energy",
          -800
        ]
      ],
      "name": "clone"
    },
    {
      "available": true,
      "in_out": [
        [
          "energy",
          10
        ],
        [
          "sunlight",
          -10
        ]
      ],
      "name": "sunlight_to_energy"
    },
    {
      "available": true,
      "in_out": [
        [
          "energy",
          50
        ],
        [
          "mineral",
          -10
        ]
      ],
      "name": "mineral_to_energy"
    },
    {
      "available": true,
      "in_out": [
        [
          "energy",
          100
        ],
        [
          "organic",
          -10
        ]
      ],
      "name": "organic_to_energy"
    },
    {
      "available": true,
      "in_out": [
        [
          "energy",
          200
        ],
        [
          "sunlight",
          -5
        ],
        [
          "water",
          -5
        ]
      ],
      "name": "sunlight_water_to_energy"
    },
    {
      "available": true,
      "in_out": [
        [
          "mineral",
          50
        ],
        [
          "sunlight",
          -5
        ],
        [
          "water",
          -5
        ]
      ],
      "name": "mineral_water_to_energy"
    }
  ],
  "regs_size": 32,
  "resources": [
    {
      "areas": [
        {
          "factor": 10.0,
          "frequency": 0.01,
          "pos": [
            500,
            300
          ],
          "radius": 20,
          "sigma": 2.0
        }
      ],
      "name": "energy",
      "stack_size": 1000
    },
    {
      "areas": [
        {
          "factor": 10.0,
          "frequency": 0.01,
          "pos": [
            0,
            0
          ],
          "radius": 350,
          "sigma": 2.0
        },
        {
          "factor": 10.0,
          "frequency": 0.01,
          "pos": [
            300,
            0
          ],
          "radius": 350,
          "sigma": 2.0
        },
        {
          "factor": 10.0,
          "frequency": 0.01,
          "pos": [
            600,
            0
          ],
          "radius": 350,
          "sigma": 2.0
        }
      ],
      "name": "sunlight",
      "stack_size": 1000
    },
    {
      "areas": [
        {
          "factor": 10.0,
          "frequency": 0.01,
          "pos": [
            100,
            300
          ],
          "radius": 50,
          "sigma": 2.0
        }
      ],
      "name": "water",
      "stack_size": 1000
    },
    {
      "areas": [
        {
          "factor": 10.0,
          "frequency": 0.01,
          "pos": [
            300,
            300
          ],
          "radius": 20,
          "sigma": 2.0
        }
      ],
      "name": "organic",
      "stack_size": 1000
    },
    {
      "areas": [
        {
          "factor": 100.0,
          "frequency": 0.001,
          "pos": [
            0,
            600
          ],
          "radius": 350,
          "sigma": 2.0
        },
        {
          "factor": 100.0,
          "frequency": 0.001,
          "pos": [
            300,
            600
          ],
          "radius": 350,
          "sigma": 2.0
        },
        {
          "factor": 100.0,
          "frequency": 0.001,
          "pos": [
            600,
            600
          ],
          "radius": 350,
          "sigma": 2.0
        }
      ],
      "name": "mineral",
      "stack_size": 1000
    }
  ],
  "seed": 0,
  "spawn_max_count": 301,
  "spawn_min_count": 100,
  "spawn_pos": [
    200,
    200
  ],
  "spawn_radius": 100,
  "x_max": 600,
  "y_max": 600
}
```



