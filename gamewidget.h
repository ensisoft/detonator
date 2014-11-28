// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft 
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"
#include "warnpush.h"
#  include <QtGui/QWidget>
#  include <QObject>
#  include <QTimer>
#  include <QElapsedTimer>
#  include <QString>
#include "warnpop.h"

#include <list>
#include <map>
#include <memory>
#include <chrono>

namespace invaders
{
    class Game;
    class Level;

    class GameWidget : public QWidget
    {
        Q_OBJECT

    public:
        // Level info for persisting level data
        struct LevelInfo {
            QString name;
            unsigned highScore;
            bool locked;
        };

        // game profile settings, for example "easy", "medium" etc.
        struct Profile {
            QString  name;
            float speed;
            unsigned spawnCount;
            unsigned spawnInterval;
            unsigned numEnemies;
        };

        GameWidget(QWidget* parent);
       ~GameWidget();

        // start a new game. index is the number of the level to play
        // and profile is the index of the difficulty profile to play
        void startGame(unsigned level, unsigned profile);

        // load level data from the specified data file
        void loadLevels(const QString& file);

        // unlock the level identified by it's name
        void unlockLevel(const QString& name);

        // restore previously stored level info 
        void setLevelInfo(const LevelInfo& info);

        // retrieve the lavel info for the level at index.
        // returns true when index is a valid index, otherwise false.
        bool getLevelInfo(LevelInfo& info, unsigned index) const;

        // set the gaming profile
        void setProfile(const Profile& profile);

        // if set to true unlocks all profiles regardless
        // of current level status.
        // if set to false restores normal level based behaviour.
        void setMasterUnlock(bool onOff);

    signals:
        void quitGame();

    private:
        virtual void timerEvent(QTimerEvent* timer) override;
        virtual void paintEvent(QPaintEvent* paint) override;
        virtual void keyPressEvent(QKeyEvent* press) override;
    private:
        bool isPaused() const;
        void showMenu();
        void showHelp();
        void quitHelp();
        void quitMenu();
        void quitScore();
        void quitLevel();

    private:
        class Animation;
        class Explosion;
        class MissileLaunch;
        class Invader;
        class Missile;
        class Background;
        class Scoreboard;        
        class Display;
        class Player;
        class Menu;
        class Help;

    private:
        std::unique_ptr<Background> background_;
        std::unique_ptr<Scoreboard> score_;
        std::unique_ptr<Display> display_;
        std::unique_ptr<Player> player_;
        std::unique_ptr<Menu> menu_;
        std::unique_ptr<Help> help_;
        std::unique_ptr<Game> game_;
        std::map<unsigned, std::unique_ptr<Invader>> invaders_;        
        std::vector<std::unique_ptr<Level>> levels_;
        std::vector<LevelInfo> info_;
        std::vector<Profile> profiles_;
        std::list<std::unique_ptr<Animation>> animations_;
        unsigned level_;
        unsigned profile_;
    private:
        QElapsedTimer timer_;
        quint64 tick_delta_;
        quint64 time_stamp_;
    private:
        bool master_unlock_;
    };

} // invaders