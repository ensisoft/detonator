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

#include "config.h"
#include "warnpush.h"
#  include <QtGui/QPaintEvent>
#  include <QtGui/QKeyEvent>
#  include <QtGui/QPen>
#  include <QtGui/QBrush>
#  include <QtGui/QColor>
#  include <QtGui/QPainter>
#  include <QtGui/QFontDatabase>
#  include <QtGui/QCursor>
#  include <QtGui/QVector2D>
#  include <QtGui/QPixmap>
#  include <QtGui/QFont>
#  include <QtGui/QFontMetrics>
#  include <QResource>
#  include <QtDebug>
#include "warnpop.h"

#include "gamewidget.h"
#include "game.h"
#include "level.h"

namespace invaders
{

// game space is discrete space from 0 to game::width() - 1 on X axis
// and from 0 to game::height() - 1 on Y axis
// we use QPoint to represent this.

// normalized widget/view space is expressed with floats from 0 to 1.0 on Y and X
// 0.0, 0.0 being the window top left and 1.0,1.0 being the window bottom right
// we use QVector2D to represent this

// eventually QPainter paint operations require coordinates in pixel space
// which runs from 0 to widget::width() on X axis and from 0 to widget::height()
// on the Y axis. this is also represented by QPoint

class TransformState 
{
public:
    TransformState(const QRect& window, int numCols, int numRows)
    {
        // divide the widget's client area int equal sized cells
        widget_ = QPoint(window.width(), window.height());
        scale_  = QPoint(window.width() / numCols, window.height() / numRows);
    }

    QRect toViewSpaceRect(const QPoint& gameTopLeft, const QPoint& gameTopBottom) const 
    {
        const QPoint top = toViewSpace(gameTopLeft);
        const QPoint bot = toViewSpace(gameTopBottom);
        return {top, bot};
    }

    QPoint toViewSpace(const QPoint& game) const
    {
        const int xpos = game.x() * scale_.x();
        const int ypos = game.y() * scale_.y();
        return { xpos, ypos };
    }

    QPoint toViewSpace(const QVector2D& norm) const 
    {
        const int xpos = widget_.x() * norm.x();
        const int ypos = widget_.y() * norm.y();
        return { xpos, ypos };
    }

    QVector2D toNormalizedViewSpace(const QPoint& p) const 
    {
        const float xpos = (float)p.x() / widget_.x();
        const float ypos = (float)p.y() / widget_.y();
        return {xpos, ypos};
    }

    QPoint getScale() const 
    { 
        return scale_; 
    }

    // get whole widget rect in widget coordinates
    QRect viewRect() const 
    { 
        return {0, 0, widget_.x(), widget_.y()}; 
    }

    // get widget width
    int viewWidth() const 
    { 
        return widget_.x();
    }

    // get widget heigth
    int viewHeight() const 
    { 
        return widget_.y(); 
    }
private:
    QPoint widget_;
    QPoint scale_;
};

class GameWidget::Animation
{
public:
    virtual ~Animation() = default;

    // returns true if animation is still valid
    // otherwise false and the animation is expired.
    virtual bool update(quint64 dt, float tick, TransformState& state) = 0;

    // 
    virtual void paint(QPainter& painter, TransformState& state) = 0;

protected:
private:
};

class GameWidget::Explosion : public GameWidget::Animation
{
public:
    Explosion(QVector2D position, quint64 lifetime) : position_(position), lifetime_(lifetime), time_(0)
    {}    

    virtual bool update(quint64 dt, float tick, TransformState& state) override
    {
        time_ += dt;
        if (time_ > lifetime_)
            return false;

        return true;
    }
    virtual void paint(QPainter& painter, TransformState& state) override
    {
        const auto pos = state.toViewSpace(position_);
        const auto dim = state.getScale();

        // explosion texture has 80 phases for the explosion
        const auto phase  = lifetime_ / 80.0;
        const int  index  = time_ / phase;

        const auto row = index / 10;
        const auto col = index % 10;

        // each explosion phase is 100x100px
        const auto w = 100;
        const auto h = 100;
        const auto x = col * w;
        const auto y = row * h;
        const QRect src(x, y, w, h);
            
        QRect dst(0, 0, 100, 100);
        dst.moveTo(pos);

        QPixmap tex = Explosion::texture();
        painter.drawPixmap(pos, tex, src);
    }
private:
    static QPixmap texture()
    {
        static QPixmap px(":/textures/ExplosionMap.png");
        return px;
    }
private:
    QVector2D position_;    
    quint64 lifetime_;
    quint64 time_;
};


class GameWidget::Invader
{
public:
    Invader(QVector2D position, QString str) : position_(position), text_(str)
    {}

    void setPosition(QVector2D position)
    {
        position_ = position;
    }

    void update(float tick, TransformState& state)
    {
        const auto unit = state.toViewSpace(QPoint(-1, 0));
        const auto off  = state.toNormalizedViewSpace(unit);
        position_ += (off * tick);
    }

    void paint(QPainter& painter, TransformState& state)
    {
        const auto dim = state.getScale();
        const auto pos = state.toViewSpace(position_);

        QPixmap tex = Invader::texture();
        const float w = tex.width();
        const float h = tex.height();
        const float a = h / w;
        QRect r(0, 0, dim.x(), dim.x() * a);
        r.moveTo(pos);
        //painter.fillRect(r, Qt::white);
        painter.drawPixmap(r, tex, tex.rect());

        QFont font;
        font.setFamily("Monospace");
        font.setPixelSize(dim.y() / 2);
        QRect rect = QFontMetrics(font).boundingRect(text_);
        //rect.moveTo(pos);
        rect.moveTo(pos + QPoint(r.width(), (r.height() - rect.height()) / 2));


        QPen pen;
        pen.setWidth(2);
        pen.setColor(Qt::darkYellow);
        painter.setFont(font);
        painter.setPen(pen);
        painter.drawText(rect, Qt::AlignCenter, text_);
    }
    QVector2D getPosition() const 
    {
        return position_;
    }
private:
    static const QPixmap& texture() 
    {
        static QPixmap px(":/textures/SpaceShipNormal.png");
        return px;
    }

private:
    QVector2D position_;
    QString text_;
};

class GameWidget::Missile
{
public:
    Missile(QVector2D pos, QString str) : position_(pos), text_(str)
    {}

    // set position in normalized widget space
    void setPosition(QVector2D pos)
    {
        position_ = pos;
    }

    void paint(QPainter& painter, TransformState& state)
    {
        const auto dim = state.getScale();
        const auto pos = state.toViewSpace(position_);

        QFont font;
        QRect rect;
        font.setFamily("Arcade");
        font.setPixelSize(dim.y() / 2);
        rect = QFontMetrics(font).boundingRect(text_);
        rect.moveTo(pos);

        QPen pen;
        pen.setWidth(2);
        pen.setColor(Qt::darkGray);
        painter.setFont(font);
        painter.setPen(pen);
        painter.drawText(rect, Qt::AlignCenter, text_);
    }
    QVector2D getPosition() const
    {
        return position_;
    }

private:
    QVector2D position_;
    QString text_;
};

//
class GameWidget::MissileLaunch : public GameWidget::Animation
{
public:
    MissileLaunch(std::unique_ptr<Invader> i,std::unique_ptr<Missile> m,
        quint64 lifetime) : invader_(std::move(i)), missile_(std::move(m)), lifetime_(lifetime), time_(0)
    {
        const auto a = missile_->getPosition();
        const auto b = invader_->getPosition();
        direction_ = b - a;
    }
    virtual bool update(quint64 dt, float tick, TransformState& state) override
    {
        time_ += dt;
        if (time_ >= lifetime_)
            return false;

        const auto d = (double)dt / (double)lifetime_;
        const auto p = direction_ * d;
        const auto n = missile_->getPosition();
        missile_->setPosition(n + p);
        invader_->update(tick, state);
        return true;
    }

    virtual void paint(QPainter& painter, TransformState& state) override
    {
        invader_->paint(painter, state);
        missile_->paint(painter, state);
    }
    QVector2D getPosition() const 
    { return invader_->getPosition(); }

private:
    std::unique_ptr<Invader> invader_;
    std::unique_ptr<Missile> missile_;
private:
    quint64 lifetime_;
    quint64 time_;
    QVector2D direction_;
};

float wrap(float max, float val, float wrap)
{
    if (val > max)
        return wrap;
    return val;
}

// game space background rendering
class GameWidget::Background
{
public:
    Background() : texture_(":textures/SpaceBackground.png")
    {
        std::srand(std::time(nullptr));

        direction_ = QVector2D(4, 3);
        direction_.normalize();

        for (std::size_t i=0; i<300; ++i)
        {
            particle p;
            p.x = (float)std::rand() / RAND_MAX;
            p.y = (float)std::rand() / RAND_MAX;
            p.a = 0.5 + (0.5 * (float)std::rand() / RAND_MAX);
            p.v = 0.05 + (0.05 * (float)std::rand() / RAND_MAX);
            p.b = !(i % 7);
            particles_.push_back(p);
        }
    }

    void paint(QPainter& painter, const QRect& rect, const QPoint&)
    {
        QBrush space(Qt::black);
        painter.fillRect(rect, space);
        painter.drawPixmap(rect, texture_, texture_.rect());

        QBrush star(Qt::white);
        QColor col(Qt::white);

        for (const auto& p : particles_)
        {
            col.setAlpha(p.a * 0xff);
            star.setColor(col);
            const auto x = p.x * rect.width();
            const auto y = p.y * rect.height();
            painter.fillRect(x, y, 1 + p.b, 1 + p.b, star);
        }
    }
    void update(quint64 time)
    {
        const auto d = direction_ * (time / 1000.0);
        for (auto& p : particles_)
        {
            p.x = wrap(1.0, p.x + d.x() * p.v, 0.0);
            p.y = wrap(1.0, p.y + d.y() * p.v, 0.0);
        }
    }
private:
    struct particle {
        float x, y;
        float a; 
        float v;
        int   b;
    };
    std::vector<particle> particles_; 
private:
    QVector2D direction_;
    QPixmap texture_;
};

class GameWidget::Scoreboard 
{
public:
    Scoreboard(unsigned finalScore, bool isHighScore, int unlockedLevel) 
    {
        text_.append("Level complete!\n");
        if (isHighScore)
            text_.append("New high score!\n");

        text_.append(QString("You scored %1 points\n\n").arg(finalScore));

        if (unlockedLevel)
            text_.append(QString("Level %1 unlocked!").arg(unlockedLevel));

        text_.append("Press Space to continue");
    }

    void paint(QPainter& painter, const QRect& area, const QPoint& scale)
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y());

        painter.setPen(pen);
        painter.setFont(font);
        painter.drawText(area, Qt::AlignCenter, text_);
    }
private:
    QString text_;
};

// Heads Up Display, display scoreboards etc.
class GameWidget::Display
{
public:
    Display(const Game& game) : game_(game), level_(0)
    {}
    void paint(QPainter& painter, const QRect& area, const QPoint& scale)
    {
        const auto& score = game_.getScore();
        const auto& text  = QString("Level %1 Score %2 Enemies %3")
            .arg(level_).arg(score.points).arg(score.pending);

        QPen pen;
        pen.setColor(Qt::darkGreen);
        pen.setWidth(1);
        painter.setPen(pen);        

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);
        painter.setFont(font);

        painter.drawText(area, Qt::AlignLeft | Qt::AlignVCenter, text);
    }
    void setLevel(unsigned number)
    { level_ = number; }

private:
    const Game& game_;
    unsigned level_;
};

// player representation on the screen
class GameWidget::Player 
{
public:
    Player() : text_(initString())
    {}

    void paint(QPainter& painter, const QRect& area, const TransformState& transform)
    {
        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(area.height() / 2);
        painter.setFont(font);        

        QFontMetrics fm(font);
        const auto width  = fm.width(text_);
        const auto height = fm.height();

        // calculate text X, Y (top left)
        const auto x = area.x() + ((area.width() - width) / 2.0);
        const auto y = area.y() + ((area.height() - height) / 2.0);

        if (false)
        {
            QPen pen;
            pen.setWidth(2);        
            pen.setColor(Qt::darkRed);
            painter.setPen(pen);
            painter.drawText(area, Qt::AlignCenter | Qt::AlignVCenter, text_);                
        }

        QPen pen;
        pen.setWidth(2);        
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        QRect rect(QPoint(x, y), QPoint(x + width, y + height));
        painter.drawRect(rect);
        painter.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, text_);

        // save the missile launch position
        missile_ = transform.toNormalizedViewSpace(QPoint(x, y));
    }

    void keyPress(QKeyEvent* press, Game& game)
    {
        if (text_ == initString())
            text_.clear();

        const auto key = press->key();
        if (key == Qt::Key_Space || key == Qt::Key_Return)
        {
            Game::missile m;
            m.position = missile_;
            m.string = text_.toLower();

            game.fire(m);
            text_.clear();
        }
        else if (key == Qt::Key_Backspace)
        {
            if (!text_.isEmpty())
                text_.resize(text_.size()-1);
        }
        else if (key >= 0x41 && key <= 0x5a)
        {
            text_.append(key);
        }
    }
    void reset()
    {
        text_ = initString();
    }
private:
    static 
    QString initString() 
    {
        static const QString init {
            "Type the correct pinyin to kill the enemies!"
        };
        return init;
    } 
    QString text_;
    QVector2D missile_;
};


// initial greeting and instructions
class GameWidget::Menu
{
public:
    Menu(const std::vector<std::unique_ptr<Level>>& levels,
         const std::vector<GameWidget::LevelInfo>& infos,
         const std::vector<GameWidget::Profile>& profiles)
    : levels_(levels), infos_(infos), profiles_(profiles), level_(0), profile_(0)
    {}

    void update(quint64 time)
    {}

    void paint(QPainter& painter, const QRect& area, const QPoint& unit)
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(unit.y() / 2);        
        painter.setFont(font);
        
        const auto cols = 7;
        const auto rows = 5;
        TransformState state(area, cols, rows);

        QRect rect;

        rect = state.toViewSpaceRect(QPoint(0, 0), QPoint(cols, 3));
        painter.drawText(rect, Qt::AlignCenter,
            "Evil chinese characters are attacking!\n"
            "Only you can stop them by typing the right pinyin.\n"
            "Good luck.\n\n"
            "Enter - Fire\n"
            "Esc - Exit\n"
            "F1 - Help\n\n"
            "Difficulty\n\n"
            "Easy  Medium  Chinese");

        rect = state.toViewSpaceRect(QPoint(0, rows-1), QPoint(cols, rows));
        painter.drawText(rect, Qt::AlignCenter,
            "Press Space to play!\n");

        QPen selected;
        selected.setWidth(2);
        selected.setColor(Qt::darkGreen);

        //rect = state.toViewSpace(QPoint(2, 3), QPoint(3, 3));

        QPen locked;
        locked.setWidth(2);
        locked.setColor(Qt::darkRed);

        font.setPixelSize(unit.y() / 2.5);
        painter.setFont(font);

        const auto prev = level_ > 0 ? level_ - 1 : levels_.size() - 1;
        const auto next = (level_ + 1) % levels_.size();
        const auto& left  = levels_[prev];
        const auto& mid   = levels_[level_];
        const auto& right = levels_[next];

        rect = state.toViewSpaceRect(QPoint(1, 3), QPoint(2, 4));
        drawLevel(painter, rect, *left, prev);

        if (infos_[level_].locked)
            painter.setPen(locked);
        else painter.setPen(selected);

        rect = state.toViewSpaceRect(QPoint(3, 3), QPoint(4, 4));
        drawLevel(painter, rect, *mid, level_);

        painter.setPen(pen);
        rect = state.toViewSpaceRect(QPoint(5, 3), QPoint(6, 4));
        painter.drawRect(rect);
        drawLevel(painter, rect, *right, next);
    }
    void keyPress(QKeyEvent* press)
    {
        const auto key = press->key();
        if (key == Qt::Key_Left)
        {
            level_ = level_ > 0 ? level_ - 1 : levels_.size() - 1;
        }
        else if (key == Qt::Key_Right)
        {
            level_ = (level_ + 1) % levels_.size();
        }
    }

    unsigned selectedLevel() const 
    { return level_; }

    unsigned selectedProfile() const 
    { return profile_; }

private:
    void drawLevel(QPainter& painter, const QRect& rect, const Level& level, int index)
    {
        const auto& info = infos_[index];

        QString locked;
        if (info.locked)
            locked = "Locked";
        else if (info.highScore)
            locked = QString("%1 points").arg(info.highScore);
        else locked = "Play!";

        painter.drawRect(rect);
        painter.drawText(rect, Qt::AlignCenter,
            QString("Level %1\n%2\n%3").arg(index+1).arg(level.name()).arg(locked));
    }

private:
    const std::vector<std::unique_ptr<Level>>& levels_;
    const std::vector<LevelInfo>& infos_;
    const std::vector<Profile>& profiles_;
    unsigned level_;
    unsigned profile_;
};

class GameWidget::Help
{
public:
    Help(const Level& level) : level_(level)
    {}

    void update(quint64 dt)
    {}

    void paint(QPainter& painter, const QRect& area, const QPoint& scale) const
    {
        QPen pen;
        pen.setWidth(1);
        pen.setColor(Qt::darkGray);
        painter.setPen(pen);

        QFont font;
        font.setFamily("Arcade");
        font.setPixelSize(scale.y() / 2);
        painter.setFont(font);

        const auto& enemies = level_.getEnemies();
        const auto cols = 3;
        const auto rows = (enemies.size() / cols) + 2;

        TransformState state(area, cols, rows);
        const auto header = state.toViewSpaceRect(QPoint(0, 0), QPoint(cols, 1));
        const auto footer = state.toViewSpaceRect(QPoint(0, rows-1), QPoint(cols, rows));

        painter.drawText(header, Qt::AlignCenter, 
            "Kill the following enemies\n");

        for (auto i=0; i<enemies.size(); ++i)
        {
            const auto& e   = enemies[i];
            const auto col  = i % cols;
            const auto row  = i / cols;
            const auto rect = state.toViewSpaceRect(QPoint(col, row + 1), QPoint(col+1, row+2));
            painter.drawText(rect, Qt::AlignHCenter | Qt::AlignTop,
                QString("%1 (%2) \n %3")
                .arg(e.string)
                .arg(e.killstring)
                .arg(e.help));
        }

        painter.drawText(footer, Qt::AlignCenter, "Press Space to play!");
    }

private:
    const Level& level_;
};


GameWidget::GameWidget(QWidget* parent) : QWidget(parent), level_(0), profile_(0), tick_delta_(0), time_stamp_(0), master_unlock_(false)
{
    QFontDatabase::addApplicationFont(":/fonts/ARCADE.TTF");

    game_.reset(new Game(20, 10));

    game_->on_invader_kill = [&](const Game::invader& i, const Game::missile& m)
    {
        auto it = invaders_.find(i.identity);

        std::unique_ptr<Invader> inv(std::move(it->second));
        std::unique_ptr<Missile> mis(new Missile(m.position, m.string.toUpper()));
        std::unique_ptr<Animation> anim(new MissileLaunch(std::move(inv), std::move(mis), 500));

        animations_.push_back(std::move(anim));
        invaders_.erase(it);
    };

    game_->on_invader_spawn = [&](const Game::invader& inv) 
    {
        const auto cols = game_->width();
        const auto rows = game_->height() + 2;
        TransformState state(rect(), cols, rows);

        const auto view = state.toViewSpace(QPoint(inv.xpos, inv.ypos));
        const auto posi = state.toNormalizedViewSpace(view);        
        std::unique_ptr<Invader> invader(new Invader(posi, inv.string));
        invaders_[inv.identity] = std::move(invader);
    };

    game_->on_invader_victory = [&](const Game::invader& inv) 
    {
        invaders_.erase(inv.identity);
    };

    game_->on_level_complete = [&](const Game::score& score)
    {
        auto& level   = info_[level_];
        auto& profile = profiles_[profile_];
        
        const auto bonus   = profile.speed;
        const auto final   = bonus * 10 * score.points;
        const bool hiscore = final > level.highScore;
        level.highScore = std::max(level.highScore, (unsigned)final);

        score_.reset(new Scoreboard(final, hiscore, 0));
    };

    background_.reset(new Background);
    display_.reset(new Display(*game_));    
    player_.reset(new Player);    

    // enable keyboard events
    setFocusPolicy(Qt::StrongFocus);
    startTimer(0);

    timer_.start();
    showMenu();
}

GameWidget::~GameWidget()
{}

void GameWidget::startGame(unsigned levelIndex, unsigned profileIndex)
{
    Q_ASSERT(levelIndex < levels_.size());
    Q_ASSERT(profileIndex < profiles_.size());

    const auto& level   = levels_[levelIndex];
    const auto& profile = profiles_[profileIndex];
    qDebug() << "Start game: " << level->name() << profile.name;

    // todo: use a deterministic seed or not?
    std::srand(std::time(nullptr));

    level->reset();
    player_->reset();
    display_->setLevel(levelIndex + 1);
    invaders_.clear();    

    Game::setup setup;
    setup.numEnemies    = profile.numEnemies;
    setup.spawnCount    = profile.spawnCount;
    setup.spawnInterval = profile.spawnInterval;
    game_->play(levels_[levelIndex].get(), setup);

    level_      = levelIndex;
    profile_    = profileIndex;
    tick_delta_ = 0;

    quitMenu();
    showHelp();
}

void GameWidget::loadLevels(const QString& file)
{
    levels_ = Level::loadLevels(file);

    for (const auto& level : levels_)
    {
        LevelInfo info;
        info.highScore = 0;
        info.name      = level->name();
        info.locked    = true;
        info_.push_back(info);
    }
    info_[0].locked = false;
}

void GameWidget::unlockLevel(const QString& name)
{
    for (auto& info : info_)
    {
        if (info.name != name)
            continue;

        info.locked = false;
        return;
    }
}

void GameWidget::setLevelInfo(const LevelInfo& info)
{
    for (auto& i : info_)
    {
        if (i.name != info.name)
            continue;

        i = info;
        return;
    }
}

bool GameWidget::getLevelInfo(LevelInfo& info, unsigned index) const 
{
    if (index >= levels_.size())
        return false;
    info = info_[index];
    return true;
}

void GameWidget::setProfile(const Profile& profile)
{
    profiles_.push_back(profile);
}

void GameWidget::setMasterUnlock(bool onOff)
{
    master_unlock_ = onOff;
}


void GameWidget::timerEvent(QTimerEvent* timer)
{
    // we divide the widget's client area into a equal sized cells
    // according to the game's size. We also add one extra row 
    // for HUD display at the top of the screen and for the player
    // at the bottom of the screen. This provides the basic layout
    // for the game in a way that doesnt depend on any actual
    // viewport size.
    const auto cols = game_->width();
    const auto rows = game_->height() + 2;

    TransformState state(rect(), cols, rows);

    // milliseconds
    const auto now  = timer_.elapsed();
    const auto time = now - time_stamp_;
    const auto tick  = 1000.0 / profiles_[profile_].speed;    
    if (!time) 
        return;

    background_->update(time);
    if (menu_) 
        menu_->update(time);    
    if (help_) 
        help_->update(time);

    // fragment of time expressed in ticks
    const auto ticks = time / tick;

    if (!isPaused())
    {
        tick_delta_ += time;
        if (tick_delta_ >= tick)
        {
            // advance game by one tick
            game_->tick();

            // synchronize invader position with the actual game position
            const auto& invaders = game_->invaders();
            for (const auto& i : invaders)
            {
                auto it   = invaders_.find(i.identity);
                auto& inv = it->second;
                const auto view = state.toViewSpace(QPoint(i.xpos, i.ypos + 1));
                const auto pos  = state.toNormalizedViewSpace(view);
                inv->setPosition(pos);
            }
            tick_delta_ = 0;
        }
        else
        {
            for (auto& pair : invaders_)
            {
                auto& invader = pair.second;
                invader->update(ticks, state);
            }
        }
    }

    // update animations
    for (auto it = std::begin(animations_); it != std::end(animations_);)
    {
        auto& anim = *it;
        if (!anim->update(time, ticks, state))
        {
            if (auto* p = dynamic_cast<MissileLaunch*>(anim.get()))
            {
                const auto pos = p->getPosition();
                std::unique_ptr<Animation> expl(new Explosion(pos, 1000));
                animations_.push_back(std::move(expl));
            }
            it = animations_.erase(it);
            continue;
        }
        ++it;
    }    

    // trigger redraw
    update();

    time_stamp_ = now;
}

void GameWidget::paintEvent(QPaintEvent* paint)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::HighQualityAntialiasing);

    // we divide the widget's client area into a equal sized cells
    // according to the game's size. We also add one extra row 
    // for HUD display at the top of the screen and for the player
    // at the bottom of the screen. This provides the basic layout
    // for the game in a way that doesnt depend on any actual
    // viewport size.
    const auto cols = game_->width();
    const auto rows = game_->height() + 2;
    const auto rect = this->rect();

    TransformState state(rect, cols, rows);

    background_->paint(painter, rect, state.getScale());

    if (menu_)
    {
        menu_->paint(painter, rect, state.getScale());
        return;
    } 
    else if (help_)
    {
        help_->paint(painter, rect, state.getScale());
        return;
    }
    else if (score_)
    {
        score_->paint(painter, rect, state.getScale());
        return;
    }

    // layout the HUD at the first "game row"
    auto top = state.toViewSpace(QPoint(0, 0));
    auto bot = state.toViewSpace(QPoint(cols, 1));
    display_->paint(painter, QRect(top, bot), state.getScale());

    // paint the invaders
    for (auto& pair : invaders_)
    {
        auto& invader = pair.second;
        invader->paint(painter, state);
    }

    // paint animations
    for (auto& anim : animations_)
    {
        anim->paint(painter, state);
    }

    // layout the player at the last "game row"
    top = state.toViewSpace(QPoint(0, rows-1));
    bot = state.toViewSpace(QPoint(cols, rows));
    player_->paint(painter, QRect(top, bot), state);
}

void GameWidget::keyPressEvent(QKeyEvent* press)
{
    const auto key = press->key();

    if (key == Qt::Key_Escape)
    {
        if (menu_)
        {
            emit quitGame();
            return;
        }
        if (score_)
            quitScore();
        if (help_)
            quitHelp();

        quitLevel();
        showMenu();        
    }
    else if (key == Qt::Key_Space)
    {
        if (menu_)
        {
            const auto level   = menu_->selectedLevel();
            const auto profile = menu_->selectedProfile();
            if (!info_[level].locked || master_unlock_)
                startGame(level, profile);
        }
        else if (score_)
        {
            quitScore();
            showMenu();
        }
        else if (help_)
        {
            quitHelp();
        }
    }
    // else if (key == Qt::Key_F1)
    // {
    //     if (game_->isRunning())
    //         showHelp();
    // }
    else if (menu_)
    {
        menu_->keyPress(press);
    }    
    else if (player_)
    {
        player_->keyPress(press, *game_);
    }

    update();
}

bool GameWidget::isPaused() const 
{
    return !!help_;
}

void GameWidget::showMenu()
{
    menu_.reset(new Menu(levels_, info_, profiles_));
}

void GameWidget::showHelp()
{
    const auto& level = *levels_[level_];
    help_.reset(new Help(level));
}
void GameWidget::quitHelp()
{
    help_.reset();
}

void GameWidget::quitMenu()
{
    menu_.reset();
}

void GameWidget::quitScore()
{
    score_.reset();
}

void GameWidget::quitLevel()
{
    if (game_->isRunning())
    {
        game_->quitLevel();
        invaders_.clear();
        animations_.clear();
    }
}

} // invaders
