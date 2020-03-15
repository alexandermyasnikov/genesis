
# GENESIS

* Author: Alexander Myasnikov
* mailto:myasnikov.alexander.s@gmail.com
* git:https://gitlab.com/amyasnikov/genesis
* site:http://amyasnikov.pro



### Цель

* Моделирование жизни



### Demo v_0.1 (youtube)

[![Demo](http://img.youtube.com/vi/umJeFR3tPzM/0.jpg)](http://www.youtube.com/watch?v=umJeFR3tPzM "Demo")



### Cell

* properpies
  * id
  * age
  * health
  * experience
  * [resource]
* type
  * resource_producer
    * При накоплении нужного количества ресурсов создается другой ресурс
    * Если это природный ресурс используется формула
      * y = factor * max(0, 1 - |t / radius| ^ sigma)
  * spore
    * При накоплении нужного количества ресурсов создается бактерия
  * defender
    * При обнаружении вражеских клеток наносит им урон
  * transfer
* cell_pipe
  * velocity
  * resource_index



### bacteria

* memory
  * code: [u8]
  * // registers_global: (ip:u32, frame:u32, registers:[u32])
  * // interrupts: [(id:u32, ip:u32, arg1:u32, arg2:u32, arg3:u32)]
  * // stack: [(ip:u32, arg1:u32, arg2:u32, arg3:u32, registers:[u32])]
* command
  * todo




### Config

```
{
  "config": {
    "position_n": 10,
    "position_max": 100,
    "resources": [
      {
        "id": 1,
        "name": "energy"
      },
      {
        "id": 2,
        "name": "water"
      }
    ],
    "areas": [
      {
        "name": "sun_area",
        "resource": "energy",
        "x0": 10,
        "x1": 20,
        "y0": 5,
        "y1": 15,
        "mean": 15,
        "sigma": 10,
        "probability": 0.1,
        "coef": 100
      }
    ],
    "recipes": [
      {
        "id": 1,
        "name": "energy_water_to_energy",
        "in": [
          ["energy", 20],
          ["water", 20]
        ],
        "out": [
          ["energy", 100]
        ]
      }
    ],
    "debug": [
      "error"
    ]
  },
  "cells": [
  ],
  "items": [
  ],
  "stats": [
  ]
}
```



