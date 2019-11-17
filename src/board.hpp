#pragma once
#include "minion.hpp"
#include <iostream>
#include <cstdlib>
#include <limits>
#include <algorithm>
using std::ostream;
using std::endl;
using std::max;

// -----------------------------------------------------------------------------
// Board state (for a single player)
// -----------------------------------------------------------------------------

inline int random(int n) {
  return std::rand() % n;
}

const int BOARDSIZE = 7;
const int NUM_EXTRA_POS = 3;

// Board state
struct Board {
  // List of minions.
  // Invariants:
  //  the first size() elements are valid, all other elements have !.exists()
  Minion minions[BOARDSIZE];
  
  int next_attacker;

  // extra positions to keep track of
  // we need enough for cleave attacks
  int track_pos[3];


  Board() : next_attacker(0) {}

  Minion const& operator [] (int i) const {
    return minions[i];
  }

  int size() const {
    for (int i=0; i<BOARDSIZE; ++i) {
      if (!minions[i].exists()) return i;
    }
    return BOARDSIZE;
  }

  bool empty() const {
    return !minions[0].exists();
  }

  bool full() const {
    return minions[BOARDSIZE-1].exists();
  }

  int total_stars() const {
    int total = 0;
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      total += minions[i].stars();
    }
    return total;
  }

  // Modification

  int append(Minion minion) {
    for (int i=0; i<BOARDSIZE; ++i) {
      if (!minions[i].exists()) {
        minions[i] = minion;
        return i;
      }
    }
    return BOARDSIZE;
  }

  bool insert(int pos, Minion minion) {
    if (full()) return false;
    std::move_backward(&minions[pos], &minions[BOARDSIZE-1], &minions[BOARDSIZE]);
    minions[pos] = minion;
    if (next_attacker > pos) {
      // Note: inserting just before the next attacker makes inserted minion the next attacker
      next_attacker++;
    }
    for (int i=0; i<NUM_EXTRA_POS; ++i) {
      if (pos >= track_pos[i]) track_pos[i]++;
    }
    return true;
  }

  void remove(int pos) {
    std::move(&minions[pos+1], &minions[BOARDSIZE], &minions[pos]);
    minions[BOARDSIZE-1].clear();
    if (pos < next_attacker) {
      next_attacker--;
    }
    for (int i=0; i<NUM_EXTRA_POS; ++i) {
      if (pos < track_pos[i]) track_pos[i]--;
      else if (pos == track_pos[i]) track_pos[i] = -1;
    }
  }

  // Targeting
  
  // target to attack
  int random_attack_target() const {
    int num_taunts = 0, num_minions = 0;
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (minions[i].taunt) num_taunts++;
      num_minions++;
    }
    if (num_taunts > 0) {
      int pick = random(num_taunts);
      for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
        if (minions[i].taunt) {
          if (pick-- == 0) return i;
        }
      }
      return -1;
    } else if (num_minions == 0) {
      return -1;
    } else {
      return random(num_minions);
    }
  }

  // minion with the lowest attack
  int lowest_attack_target() const {
    int num_lowest = 0, lowest_attack = std::numeric_limits<int>::max();
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (minions[i].attack < lowest_attack) {
        num_lowest = 1;
        lowest_attack = minions[i].attack;
      } else if (minions[i].attack == lowest_attack) {
        num_lowest++;
      }
    }
    int pick = random(num_lowest);
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (minions[i].attack == lowest_attack) {
        if (pick-- == 0) return i;
      }
    }
    return -1;
  }

  int random_living_minion() const {
    int num_alive = 0;
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (!minions[i].dead()) {
        num_alive++;
      }
    }
    if (num_alive == 0) return -1;
    int pick = random(num_alive);
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (!minions[i].dead()) {
        if (pick-- == 0) return i;
      }
    }
    return -1;
  }

  template <typename F>
  void for_each_alive(F fun) {
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (!minions[i].dead()) {
        fun(minions[i]);
      }
    }
  }
  template <typename F>
  void for_each_alive(F fun) const {
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (!minions[i].dead()) {
        fun(minions[i]);
      }
    }
  }
  template <typename F>
  void for_each_with_pos(F fun) {
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (!minions[i].dead()) {
        fun(i, minions[i]);
      }
    }
  }

  // Permanent buffs

  template <typename Condition>
  void buff_all_if(int attack, int health, Condition c) {
    for_each_alive([&](Minion& m){
      if (c(m)) m.buff(attack,health);
    });
  }
  void buff_all(int attack, int health) {
    for_each_alive([&](Minion& m){
      m.buff(attack,health);
    });
  }

  // Specific buffs

  void give_random_minion_divine_shield() {
    int i = random_living_minion();
    if (i != -1) minions[i].divine_shield = true;
  }
  void buff_random_minion(int attack, int health) {
    int i = random_living_minion();
    if (i != -1) minions[i].buff(attack, health);
  }

  // Duplication effects

  int extra_summon_count() const {
    return has_minion(MinionType::Khadgar) + 1;
  }
  int extra_deathrattle_count() const {
    return has_minion(MinionType::BaronRivendare) + 1;
  }
  int extra_battlecry_count() const {
    return has_minion(MinionType::BrannBronzebeard) + 1;
  }
  // is the minion present? return 0 if not, 1 if yes, 2 if golden
  int has_minion(MinionType type) const {
    int have = 0;
    for_each_alive([type,&have](Minion const& m) {
      if (m.type == type) have = max(have, m.golden ? 2 : 1);
    });
    return have;
  }

  // Auras

  void recompute_auras() {
    for_each_alive([&](Minion& m) {
      m.clear_aura_buff();
    });
    for_each_with_pos([&](int pos, Minion& m){
      recompute_aura_from(m, pos);
    });
  }
  template <typename Condition>
  void aura_buff_others_if(int attack, int health, int pos, Condition c) {
    // Note: also buff "dead" minions, they might not die because of the buff
    for (int i=0; i<BOARDSIZE && minions[i].exists(); ++i) {
      if (i != pos && c(minions[i])) minions[i].aura_buff(attack,health);
    }
  }
  void aura_buff_adjacent(int attack, int health, int pos) {
    if (pos > 0 && minions[pos-1].exists()) minions[pos-1].aura_buff(attack,health);
    if (pos+1 < BOARDSIZE && minions[pos+1].exists()) minions[pos+1].aura_buff(attack,health);
  }

private:
  void recompute_aura_from(Minion& m, int pos);
  void on_friendly_summon(Minion& m, Minion& summoned); // summoning outside battle?
};

inline ostream& operator << (ostream& s, Board const& b) {
  for (int i=0; i<BOARDSIZE; ++i) {
    if (b.minions[i].exists()) {
      s << b.minions[i] << endl;
    }
  }
  return s;
}
