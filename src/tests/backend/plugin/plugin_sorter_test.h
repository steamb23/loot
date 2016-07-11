/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2016    WrinklyNinja

This file is part of LOOT.

LOOT is free software: you can redistribute
it and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

LOOT is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with LOOT.  If not, see
<http://www.gnu.org/licenses/>.
*/

#ifndef LOOT_TESTS_BACKEND_PLUGIN_PLUGIN_SORTER_TEST
#define LOOT_TESTS_BACKEND_PLUGIN_PLUGIN_SORTER_TEST

#include "backend/plugin/plugin_sorter.h"

#include "tests/backend/base_game_test.h"

namespace loot {
namespace test {
class PluginSorterTest : public BaseGameTest {
protected:
  inline virtual void SetUp() {
    BaseGameTest::SetUp();

    game_ = Game(GetParam());
    game_.SetGamePath(dataPath.parent_path());
    ASSERT_NO_THROW(game_.Init(false, localPath));
  }

  Game game_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        PluginSorterTest,
                        ::testing::Values(
                          GameType::tes4));

TEST_P(PluginSorterTest, sortingWithNoLoadedPluginsShouldReturnAnEmptyList) {
  PluginSorter sorter;
  std::list<Plugin> sorted = sorter.Sort(game_, Language::Code::english);

  EXPECT_TRUE(sorted.empty());
}

TEST_P(PluginSorterTest, sortingShouldNotMakeUnnecessaryChangesToAnExistingLoadOrder) {
  ASSERT_NO_THROW(game_.LoadPlugins(false));

  PluginSorter ps;
  std::list<std::string> expectedSortedOrder = getLoadOrder();

  std::list<Plugin> sorted = ps.Sort(game_, Language::Code::english);
  EXPECT_TRUE(std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));

  // Check stability.
  sorted = ps.Sort(game_, Language::Code::english);
  EXPECT_TRUE(std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));
}

TEST_P(PluginSorterTest, sortingShouldClearExistingGameMessages) {
  ASSERT_NO_THROW(game_.LoadPlugins(false));
  game_.AppendMessage(Message(Message::Type::say, "1"));
  ASSERT_FALSE(game_.GetMessages().empty());

  PluginSorter ps;
  std::list<Plugin> sorted = ps.Sort(game_, Language::Code::english);
  EXPECT_TRUE(game_.GetMessages().empty());
}

TEST_P(PluginSorterTest, failedSortShouldNotClearExistingGameMessages) {
  ASSERT_NO_THROW(game_.LoadPlugins(false));
  PluginMetadata plugin(blankEsm);
  plugin.LoadAfter({File(blankMasterDependentEsm)});
  game_.GetUserlist().AddPlugin(plugin);
  game_.AppendMessage(Message(Message::Type::say, "1"));
  ASSERT_FALSE(game_.GetMessages().empty());

  PluginSorter ps;
  EXPECT_ANY_THROW(ps.Sort(game_, Language::Code::english));
  EXPECT_FALSE(game_.GetMessages().empty());
}

TEST_P(PluginSorterTest, sortingShouldEvaluateRelativePriorities) {
  ASSERT_NO_THROW(game_.LoadPlugins(false));
  PluginMetadata plugin(blankDifferentMasterDependentEsp);
  plugin.Priority(-100000);
  plugin.SetPriorityGlobal(true);
  game_.GetUserlist().AddPlugin(plugin);

  PluginSorter ps;
  std::list<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentMasterDependentEsp,
      blankEsp,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankPluginDependentEsp,
      blankDifferentPluginDependentEsp,
  });

  std::list<Plugin> sorted = ps.Sort(game_, Language::Code::english);
  EXPECT_TRUE(std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));
}

TEST_P(PluginSorterTest, sortingWithPrioritiesShouldInheritRecursivelyRegardlessOfEvaluationOrder) {
  ASSERT_NO_THROW(game_.LoadPlugins(false));

  // Set Blank.esp's priority.
  PluginMetadata plugin(blankEsp);
  plugin.Priority(2);
  game_.GetUserlist().AddPlugin(plugin);

  // Load Blank - Master Dependent.esp after Blank.esp so that it
  // inherits Blank.esp's priority.
  plugin = PluginMetadata(blankMasterDependentEsp);
  plugin.LoadAfter({
      File(blankEsp),
  });
  game_.GetUserlist().AddPlugin(plugin);

  // Load Blank - Different.esp after Blank - Master Dependent.esp, so
  // that it inherits its inherited priority.
  plugin = PluginMetadata(blankDifferentEsp);
  plugin.LoadAfter({
      File(blankMasterDependentEsp),
  });
  game_.GetUserlist().AddPlugin(plugin);

  // Set Blank - Different Master Dependent.esp to have a higher priority
  // than 0 but lower than Blank.esp. Need to also make it a global priority
  // because it doesn't otherwise conflict with the other plugins.
  plugin = PluginMetadata(blankDifferentMasterDependentEsp);
  plugin.Priority(1);
  plugin.SetPriorityGlobal(true);
  game_.GetUserlist().AddPlugin(plugin);

  PluginSorter ps;
  std::list<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentMasterDependentEsp,
      blankEsp,
      blankMasterDependentEsp,
      blankDifferentEsp,
      blankPluginDependentEsp,
      blankDifferentPluginDependentEsp,
  });

  std::list<Plugin> sorted = ps.Sort(game_, Language::Code::english);
  EXPECT_TRUE(std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));
}

TEST_P(PluginSorterTest, sortingShouldUseLoadAfterMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(game_.LoadPlugins(false));
  PluginMetadata plugin(blankEsp);
  plugin.LoadAfter({
      File(blankDifferentEsp),
      File(blankDifferentPluginDependentEsp),
  });
  game_.GetUserlist().AddPlugin(plugin);

  PluginSorter ps;
  std::list<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankDifferentPluginDependentEsp,
      blankEsp,
      blankPluginDependentEsp,
  });

  std::list<Plugin> sorted = ps.Sort(game_, Language::Code::english);
  EXPECT_TRUE(std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));
}

TEST_P(PluginSorterTest, sortingShouldUseRequirementMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(game_.LoadPlugins(false));
  PluginMetadata plugin(blankEsp);
  plugin.Reqs({
      File(blankDifferentEsp),
      File(blankDifferentPluginDependentEsp),
  });
  game_.GetUserlist().AddPlugin(plugin);

  PluginSorter ps;
  std::list<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankDifferentPluginDependentEsp,
      blankEsp,
      blankPluginDependentEsp,
  });

  std::list<Plugin> sorted = ps.Sort(game_, Language::Code::english);
  EXPECT_TRUE(std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));
}

TEST_P(PluginSorterTest, sortingShouldThrowIfACyclicInteractionIsEncountered) {
  ASSERT_NO_THROW(game_.LoadPlugins(false));
  PluginMetadata plugin(blankEsm);
  plugin.LoadAfter({File(blankMasterDependentEsm)});
  game_.GetUserlist().AddPlugin(plugin);

  PluginSorter ps;
  EXPECT_ANY_THROW(ps.Sort(game_, Language::Code::english));
}
}
}

#endif
