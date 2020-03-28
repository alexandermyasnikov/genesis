
# GENESIS

* Author: Alexander Myasnikov
* mailto:myasnikov.alexander.s@gmail.com
* git:https://gitlab.com/amyasnikov/genesis
* site:http://amyasnikov.pro



### Цель

* Моделирование жизни



### Demo (youtube)

[![Demo](http://img.youtube.com/vi/-uerfX1w4SQ/0.jpg)](https://www.youtube.com/watch?v=-uerfX1w4SQ "Demo v0.2")



### World

* Мир - прямоугольная область, состоящая из клеток.
* Присутствуют области с ресурсами, которые боты могут извлекать.
* В одной клетке может находиться один бот.



### Bot (Microbe)

* Бот содержит геном (код), которые определяет поведение бота.
* У бота есть хранилице ресурсов.
* Ресурсы могут создаваться из других ресурсов или извекаться из вне.
* Бот может перемещаться, добывать/создавать ресурсы, клонировать себя, манипулировать со своим геномом.
* Время жизни бота ограничено.



### TODO

* Добавить команды атаки и другие.
* Добавить возможность определять родственные геномы.
* Добавить возможность реагировать бота на раздражения (прерывания).



### Config

* Все параметры вынесены в файл конфига.
* Пример файла:

```
{
  "age_max": 1000,
  "areas": {
    "sunlight": [
      {
        "factor": 1.0,
        "pos": [
          300,
          500
        ],
        "radius": 50,
        "sigma": 2.0
      },
      {
        "factor": 1.0,
        "pos": [
          500,
          300
        ],
        "radius": 250,
        "sigma": 2.0
      },
      {
        "factor": 1.0,
        "pos": [
          200,
          200
        ],
        "radius": 150,
        "sigma": 2.0
      }
    ]
  },
  "code_size": 64,
  "debug": [
    "_mind ",
    "_trace",
    "debug",
    "error",
    "time"
  ],
  "energy_remaining": 5,
  "interval_save_world_ms": 600000,
  "interval_update_world_ms": 1,
  "recipes": [
    {
      "available": true,
      "in_out": {
        "energy": 10,
        "sunlight": -10
      },
      "name": "sunlight_to_energy"
    },
    {
      "available": true,
      "in_out": {
        "sunlight": 10
      },
      "name": "sunlight_extract"
    }
  ],
  "resources": [
    {
      "extractable": false,
      "name": "energy",
      "stack_size": 1000
    },
    {
      "extractable": true,
      "name": "sunlight",
      "stack_size": 1000
    },
    {
      "extractable": false,
      "name": "water",
      "stack_size": 1000
    }
  ],
  "seed": 0,
  "spawn_max_count": 501,
  "spawn_min_count": 500,
  "spawn_pos": [
    200,
    200
  ],
  "spawn_radius": 50,
  "x_max": 600,
  "y_max": 600
}
```



