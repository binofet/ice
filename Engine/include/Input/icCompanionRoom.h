#ifndef IC_COMPANION_ROOM_H
#define IC_COMPANION_ROOM_H
#include <cstdint>
#include <string>
#include <vector>
class icCompanionRoom {
public:
    enum class Phase { Lobby, Playing, Results };
    struct Player {std::uint64_t id=0;std::string name;unsigned score=0,balls=0;bool online=true,forfeited=false;double seen=0;};
    std::uint64_t Join(const std::string& name,double now);
    bool Touch(std::uint64_t id,double now);
    void Leave(std::uint64_t id);
    void Poll(double now);
    bool Start(std::uint64_t leader);
    bool BeginShot(std::uint64_t player);
    bool FinishShot(std::uint64_t player,unsigned points);
    bool CanShoot(std::uint64_t player) const;
    void Interrupt(); // invalidate an unfinished round after host suspension
    Phase State() const {return m_phase;}
    std::uint64_t Current() const;
    std::uint64_t Leader() const;
    unsigned Turn() const {return m_turn;}
    unsigned Round() const {return m_round;}
    bool InFlight() const {return m_shooter!=0;}
    const std::vector<Player>& Players() const {return m_players;}
private:
    Player* Find(std::uint64_t id);
    void Advance();
    std::vector<Player> m_players;
    Phase m_phase=Phase::Lobby;
    std::uint64_t m_next=1,m_current=0,m_shooter=0;
    unsigned m_round=0,m_turn=0;
};
#endif
