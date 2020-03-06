# GENESIS

* Author: Alexander Myasnikov
* mailto:myasnikov.alexander.s@gmail.com
* git:https://gitlab.com/amyasnikov/genesis
* site:http://amyasnikov.pro



### Цель

* Смоделировать игру Labyrinth (paper-and-pencil game) с самообучением



### Demo v_0.1 (youtube)

[![Demo](http://img.youtube.com/vi/umJeFR3tPzM/0.jpg)](http://www.youtube.com/watch?v=umJeFR3tPzM "Demo")



### Bot

* Memory
  * code: [u8]
  * registers_global: (ip:u32, frame:u32, registers:[u32])
  * interrupts: [(id:u32, ip:u32, arg1:u32, arg2:u32, arg3:u32)]
  * stack: [(ip:u32, arg1:u32, arg2:u32, arg3:u32, registers:[u32])]
* Properpy
  * value
  * min
  * max
  * id
  * name
* Properpies: vector<prop_t>
  * age
  * experience
  * health
  * energy_step
  * inventory
    * hit
    * shot
  * // direction
* Interrupts
  * MSG
  * BOT_NEAR
  * ITEM_NEAR
  * PROP_CHANGE <id> <min> <max>
* Events
  * MSG <id> <arg1> <arg2> <arg3>
  * BOT_NEAR <direction> <distance>
  * ITEM_NEAR <direction> <distance>
  * PROP_CHANGE <id>
* Commands
  * MOVE_REL <direction>
  * MOVE_ABS <direction>
  * ATTACK <type> <direction>
  * GET_PROP <id> <res>
  * MUTATION
  * TILE_INFO <direction> <res>
  * MSG <id> <arg1> <arg2> <arg3>
* Commands RISC
  * NOP
  * RET
  * BR   <offset>
  * SET  <dst> <value>
  * MOV  <dst> <src>
  * ADD  <dst> <src1> <src2>
  * SUB  <dst> <src1> <src2>
  * MULT <dst> <src1> <src2>
  * DIV  <dst> <src1> <src2>
  * IF   <op>  <src1> <src2> <offset>
  * SYSCALL <id> <arg1> <arg2> <arg3>
    * <rand> <dst>
    * <time> <dst>



# Item

* Type
  * Portal
  * Wall
  * Trap
  * First_aid_kit
  * Ammunition
* impassable



### World

* std::vector<event_t>
* std::vector<std::variant<bot_t, item_t>>
* system_t::update(world_t& world)
* system_bot_t::update(bot_t& bot)



### Config

```
{
  "config": {
   "position_n": 10,
   "position_max": 100,
   "properpies": [
   ],
   "debug": [
     "error"
   ]
  },
  "bots": [
  ],
  "items": [
  ],
  "stats": [
  ]
}
```


