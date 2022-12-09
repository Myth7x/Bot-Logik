#include "stdafx.h"

#include "utils.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "bot_manager.h"
#include "sectree.h"
#include "sectree_manager.h"

#include "companion_action_lengths.h"
void CBotManager::CompanionStateEffect(DWORD bid)
{
	std::map<DWORD, CBot>::iterator m_bot = mBots.find(bid);
	if (m_bot == mBots.end())
		return;

	
	const LPCHARACTER& ch = CHARACTER_MANAGER::instance().Find(m_bot->second.dwBotID);
	const LPCHARACTER& owner_copy = CHARACTER_MANAGER::instance().FindByPID(m_bot->second.dwOwner);
	
	DWORD current_time = get_dword_time();

	//if (m_bot->second.companion_config.dwLastEffectUpdate3+ 199 < current_time)
	//{
	//	m_bot->second.companion_config.dwLastEffectUpdate3 = current_time;
	//	//owner_copy->CreateFly(FLY_OWNER_TO_BOT, ch);
	//	owner_copy->CreateFly(FLY_OWNER_TO_BOT, ch);
	//}

	//
	//if (m_bot->second.companion_config.dwLastEffectUpdate2 + 999 < current_time)
	//{
	//	m_bot->second.companion_config.dwLastEffectUpdate2 = current_time;
	//	//owner_copy->CreateFly(FLY_OWNER_TO_BOT, ch);
	//	ch->CreateFly(FLY_BOT_TO_OWNER, owner_copy);
	//}

	
	if (m_bot->second.companion_config.dwLastEffectUpdate + 99 < current_time)
	{
		m_bot->second.companion_config.dwLastEffectUpdate = current_time;

		ch->EffectPacket(SE_BOT);
		ch->EffectPacket(GetCompanionStateEffect(m_bot->first, 0));
		ch->EffectPacket(GetCompanionStateEffect(m_bot->first, 1));
		ch->EffectPacket(GetCompanionStateEffect(m_bot->first, 2));
	}
	

	return;
}

DWORD CBotManager::GetCompanionStateEffect(DWORD bot, DWORD type)
{
	std::map<DWORD, CBot>::iterator m_bot = mBots.find(bot);
	if (m_bot == mBots.end())
		return 0;
	// Behavior
	if (type == 0)
	{
		switch (m_bot->second.companion_config.dwStateBehavior)
		{
		case COMPANION_STATE_BEHAVIOR_IDLE:
			return SE_BOT_COMPANION_STATE_BEHAVIOR_IDLE;
		case COMPANION_STATE_BEHAVIOR_DEFENSIVE:
			return SE_BOT_COMPANION_STATE_BEHAVIOR_DEFENSIVE;
		case COMPANION_STATE_BEHAVIOR_AGGRESSIVE:
			return COMPANION_STATE_BEHAVIOR_AGGRESSIVE;
		}
	}
	// Combat
	else if (type == 1)
	{
		switch (m_bot->second.companion_config.dwStateCombat)
		{
		case COMPANION_STATE_COMBAT_IDLE:
			return SE_BOT_COMPANION_STATE_COMBAT_IDLE;
		case COMPANION_STATE_COMBAT_ATTACK:
			return SE_BOT_COMPANION_STATE_COMBAT_ATTACK;
		case COMPANION_STATE_COMBAT_RETREAT:
			return SE_BOT_COMPANION_STATE_COMBAT_RETREAT;
		}
	}
	// Movement
	else if (type == 2)
	{
		switch (m_bot->second.companion_config.dwStateMovement)
		{
		case COMPANION_STATE_MOVEMENT_IDLE:
			return SE_BOT_COMPANION_STATE_MOVEMENT_IDLE;
		case COMPANION_STATE_MOVEMENT_FOLLOW:
			return SE_BOT_COMPANION_STATE_MOVEMENT_FOLLOW;
		}
	}
	return -1;
}



struct FFindFindTarget
{
	FFindFindTarget(LPCHARACTER center, LPCHARACTER attacker, const CHARACTER_SET& excepts_set = empty_set_)
		: m_pkChrCenter(center),
		m_pkChrNextTarget(NULL),
		m_pkChrAttacker(attacker),
		m_count(0),
		m_excepts_set(excepts_set)
	{
	}

	void operator ()(LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER))
			return;

		LPCHARACTER pkChr = (LPCHARACTER)ent;

		if (!m_excepts_set.empty()) {
			if (m_excepts_set.find(pkChr) != m_excepts_set.end())
				return;
		}

		if (m_pkChrCenter == pkChr)
			return;


		if (abs(m_pkChrCenter->GetX() - pkChr->GetX()) > 1000 || abs(m_pkChrCenter->GetY() - pkChr->GetY()) > 1000)
			return;

		float fDist = DISTANCE_APPROX(m_pkChrCenter->GetX() - pkChr->GetX(), m_pkChrCenter->GetY() - pkChr->GetY());

		if (fDist < 1000)
		{
			++m_count;

			if ((m_count == 1) || number(1, m_count) == 1)
				m_pkChrNextTarget = pkChr;
		}
	}

	LPCHARACTER GetVictim()
	{
		return m_pkChrNextTarget;
	}

	LPCHARACTER m_pkChrCenter;
	LPCHARACTER m_pkChrNextTarget;
	LPCHARACTER m_pkChrAttacker;
	int		m_count;
	const CHARACTER_SET& m_excepts_set;
private:
	static CHARACTER_SET empty_set_;
};

CHARACTER_SET FFindFindTarget::empty_set_;


bool CBotManager::PostCompanionUpdate(DWORD bid)
{
	std::map<DWORD, CBot>::iterator m_bot = mBots.find(bid);
	if (m_bot == mBots.end())
		return false;
	CompanionStateEffect(m_bot->first);
	return true;
}


bool CBotManager::UpdateCompanionStateBehavior(DWORD time, std::map<DWORD, CBot>::iterator m_bot)
{
	const LPCHARACTER& lpc_bot = CHARACTER_MANAGER::instance().Find(m_bot->second.dwBotID);
	const LPCHARACTER& lpc_owner = CHARACTER_MANAGER::instance().FindByPID(m_bot->second.dwOwner);

	switch (m_bot->second.companion_config.dwStateBehavior)
	{


	case COMPANION_STATE_BEHAVIOR_IDLE:
		// Follow only, no attack target, no attack harming
		if (m_bot->second.companion_config.dwStateMovement != COMPANION_STATE_MOVEMENT_FOLLOW)
		{
			m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_FOLLOW;
			m_bot->second.dwTarget = lpc_owner->GetVID();
		}
		return UpdateCompanionStateCombat(time, m_bot);

		
	case COMPANION_STATE_BEHAVIOR_DEFENSIVE:
		// Follow only, attack target, no attack harming
		if (m_bot->second.companion_config.dwStateMovement != COMPANION_STATE_MOVEMENT_FOLLOW)
		{
			if (DISTANCE_APPROX(lpc_bot->GetX() - lpc_owner->GetX(), lpc_bot->GetY() - lpc_owner->GetY()) > MIN_DISTANCE_TO_OWNER)
			{
				m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_FOLLOW;
				m_bot->second.dwTarget = lpc_owner->GetVID();
			}
			else
				m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_IDLE;
			
		}
		return UpdateCompanionStateCombat(time, m_bot);


	}

	return false;
}

bool CBotManager::UpdateCompanionStateCombat(DWORD time, std::map<DWORD, CBot>::iterator m_bot)
{
	const LPCHARACTER& lpc_bot = CHARACTER_MANAGER::instance().Find(m_bot->second.dwBotID);
	const LPCHARACTER& lpc_owner = CHARACTER_MANAGER::instance().FindByPID(m_bot->second.dwOwner);
	const LPCHARACTER& lpc_owner_victim = lpc_owner->GetVictim();
	const LPCHARACTER& lpc_owner_target = lpc_owner->GetTarget();
	const LPCHARACTER& lpc_companion_victim = lpc_bot->GetVictim();
	const LPCHARACTER& lpc_companion_target = lpc_bot->GetTarget();


	CEntity::ENTITY_MAP map_view = lpc_owner->GetCurrentView();
	std::map<DWORD, DWORD> map_target;
	std::multimap<DWORD, DWORD> map_target_sorted;

	LPCHARACTER lpc_target = NULL;
	float fDistance = 0.f, fV1 = 0.f, fV2 = 0.f, fV3=0.f;
	bool bHarmingOwner = false;
	DWORD dwHarmLevel = 0;

	/* COMPANION_STATE_COMBAT_ATTACK */
	if (COMPANION_STATE_COMBAT_ATTACK == m_bot->second.companion_config.dwStateCombat)
	{
		lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Behaivour Defensive, Combat State: Attack START: %d", m_bot->second.companion_config.dwStateBehavior);

		lpc_target = CHARACTER_MANAGER::Instance().Find(m_bot->second.dwTarget);

		if (!lpc_target || lpc_target->IsDead())
		{
			m_bot->second.companion_config.dwStateCombat = COMPANION_STATE_COMBAT_IDLE;
			m_bot->second.target_attack.dwVID = 0;
			m_bot->second.target_attack.dwStartTime = 0;
			return true;
		}

		fV1 = DISTANCE_APPROX(lpc_bot->GetX() - lpc_target->GetX(), lpc_bot->GetY() - lpc_target->GetY());

		if (fV1 <= 170.f)
		{
			if (!Attack(lpc_bot, lpc_target))
			{
				m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_IDLE;
				m_bot->second.companion_config.dwStateCombat = COMPANION_STATE_COMBAT_IDLE;
				lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Behaivour Defensive, Combat State: Attack END2: %d", m_bot->second.companion_config.dwStateBehavior);
				return UpdateCompanionStateMovement(time, m_bot);
			}
			else
			{
				m_bot->second.companion_config.dwStateCombat = COMPANION_STATE_COMBAT_IDLE;
				//m_bot->second.dwTarget = lpc_target->GetVID();
				lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Behaivour Defensive, Combat State: Attack END1: %d", m_bot->second.companion_config.dwStateBehavior);
				return true;
			}
			
			
		}
		m_bot->second.companion_config.dwStateCombat	= COMPANION_STATE_COMBAT_IDLE;
		m_bot->second.companion_config.dwStateMovement	= COMPANION_STATE_MOVEMENT_FOLLOW;
		//m_bot->second.dwTarget = lpc_target->GetVID();
		return UpdateCompanionStateMovement(time, m_bot);
	}
	
	/* COMPANION_STATE_COMBAT_IDLE */
	else if (COMPANION_STATE_COMBAT_IDLE == m_bot->second.companion_config.dwStateCombat)
	{
		lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Behaivour Defensive, Combat State: Idle START: %d", m_bot->second.companion_config.dwStateBehavior);

		switch (m_bot->second.companion_config.dwStateBehavior)
		{
		case COMPANION_STATE_BEHAVIOR_DEFENSIVE:
			if (m_bot->second.target_attack.dwVID > 0)
			{
				m_bot->second.companion_config.dwStateCombat = COMPANION_STATE_COMBAT_ATTACK;
				m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_IDLE;
				return true;
			}
			/*
				New Logic:
					We iterate the owners view, and build a list of targets with priority
			*/
			for (std::pair<const LPENTITY, int> it : map_view)
			{
				dwHarmLevel = 0;
				const LPCHARACTER& lpc_entity = (LPCHARACTER)it.first;
				fV1 = (DWORD)DISTANCE_APPROX(lpc_owner->GetX() - lpc_entity->GetX(), lpc_owner->GetY() - lpc_entity->GetY());
				dwHarmLevel += (DWORD)10 / (fV1 / 100);
				//if (fV1 <= 600)
				//{
				//	dwHarmLevel = fV1;
				//} 
				//
				//if (lpc_entity->GetTarget() == lpc_owner)
				//{
				//	dwHarmLevel++;
				//}

				if (dwHarmLevel > 0 && dwHarmLevel <= 2000)
				{
					map_target.insert(std::pair<DWORD, DWORD>((DWORD)lpc_entity->GetVID(), dwHarmLevel));
					continue;
				}
			}
			if (map_target.size() > 0)
			{
				for (std::pair<const DWORD, DWORD> it : map_target)
				{
					map_target_sorted.insert(std::pair<DWORD, DWORD>(it.second, it.first));
				}

				if (map_target_sorted.size() > 0)
				{
					m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_FOLLOW;
					//m_bot->second.dwTarget = map_target_sorted.begin()->second;

					m_bot->second.target_attack.dwVID = map_target_sorted.begin()->second;
					m_bot->second.target_attack.dwStartTime = get_dword_time();
				}
			}

			if (m_bot->second.dwTarget > 0)
			{
				const LPCHARACTER& lpc_entity = CHARACTER_MANAGER::Instance().Find(m_bot->second.dwTarget);
				
				m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_FOLLOW;
				m_bot->second.companion_config.dwStateCombat	= COMPANION_STATE_COMBAT_ATTACK;

				lpc_bot->SetRotationToXY(lpc_entity->GetX(), lpc_entity->GetY());

				lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Behaivour Defensive, Combat State: Idle END1: %d", m_bot->second.companion_config.dwStateBehavior);
				return UpdateCompanionStateMovement(time, m_bot);
			}

			// Reset to owner
			m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_FOLLOW;
			m_bot->second.dwTarget = lpc_owner->GetVID();

			lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Behaivour Defensive, Combat State: Idle ENDx: %d", m_bot->second.companion_config.dwStateBehavior);
			
			return UpdateCompanionStateMovement(time, m_bot);

		case COMPANION_STATE_BEHAVIOR_IDLE:
			lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Behaivour Defensive, Combat State: Idle ENDx: %d", m_bot->second.companion_config.dwStateBehavior);
			return UpdateCompanionStateMovement(time, m_bot);
		}
	}

	lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Behaivour Defensive, Combat State: Idle ENDn: %d", m_bot->second.companion_config.dwStateBehavior);
	return false;
}

bool CBotManager::UpdateCompanionStateMovement(DWORD time, std::map<DWORD, CBot>::iterator m_bot)
{
	const LPCHARACTER& lpc_bot = CHARACTER_MANAGER::instance().Find(m_bot->second.dwBotID);
	const LPCHARACTER& lpc_owner = CHARACTER_MANAGER::instance().FindByPID(m_bot->second.dwOwner);
	const LPCHARACTER& lpc_owner_victim = lpc_owner->GetVictim();
	const LPCHARACTER& lpc_owner_target = lpc_owner->GetTarget();
	const LPCHARACTER& lpc_companion_victim = lpc_bot->GetVictim();
	const LPCHARACTER& lpc_companion_target = lpc_bot->GetTarget();

	/* COMPANION_STATE_MOVEMENT_FOLLOW */
	if (COMPANION_STATE_MOVEMENT_FOLLOW == m_bot->second.companion_config.dwStateMovement)
	{
		switch (m_bot->second.companion_config.dwStateBehavior)
		{
			case COMPANION_STATE_BEHAVIOR_DEFENSIVE:
			{
				if (m_bot->second.dwTarget == m_bot->second.dwOwner)
				{
					if (_FollowOwner(m_bot->second))
					{
					}
					else
					{
						if (lpc_bot->IsStateMove())
						{
							m_bot->second.movement.dwLastMoveStartTime = 0;
							m_bot->second.movement.dwLastMoveTime = 0;
							
						}
						m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_IDLE;
					}
				}
				else
				{
					if (_FollowTarget(m_bot->second, CHARACTER_MANAGER::Instance().Find(m_bot->second.dwTarget)))
					{
					}
					else
					{
						if (lpc_bot->IsStateMove())
						{
							m_bot->second.movement.dwLastMoveStartTime = 0;
							m_bot->second.movement.dwLastMoveTime = 0;
							
						}
						m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_IDLE;
					}
				}
				
				return true;
			}

			case COMPANION_STATE_BEHAVIOR_IDLE:
			{
				if (_FollowOwner(m_bot->second))
				{
					//lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Companion is following you..");
				}
				else
				{
					//lpc_owner->ChatPacket(CHAT_TYPE_INFO, "Companion is idling beside you..");
					if (lpc_bot->IsStateMove())
					{
						m_bot->second.movement.dwLastMoveStartTime = 0;
						m_bot->second.movement.dwLastMoveTime = 0;
						//lpc_owner->ChatPacket(CHAT_TYPE_INFO, "BOT MOVEMENT STATE FOLLOW -> IDLE");
						
					}
					m_bot->second.companion_config.dwStateMovement = COMPANION_STATE_MOVEMENT_IDLE;
				}
				return true;
			}
		}
	}
	
	/* COMPANION_STATE_MOVEMENT_IDLE */
	else if (COMPANION_STATE_MOVEMENT_IDLE == m_bot->second.companion_config.dwStateMovement)
	{
		return true;
	}

	return false;
}


bool CBotManager::CompanionUpdateState(DWORD bid)
{
	std::map<DWORD, CBot>::iterator m_bot = mBots.find(bid);
	if (m_bot == mBots.end())
		return false;

	Companion_Logic_Container * clc = &m_bot->second.companion_config.logic_container;
	if (!&clc)
	{
		sys_err("CompanionUpdateState: no logic container for bot %u", bid);
		return false;
	}

	const LPCHARACTER& lpc_bot = CHARACTER_MANAGER::instance().Find(m_bot->second.dwBotID);
	if (!lpc_bot)
	{
		sys_err("CompanionUpdateState: no bot %u", m_bot->second.dwBotID);
		return false;
	}
	
	const LPCHARACTER& lpc_owner = CHARACTER_MANAGER::instance().FindByPID(m_bot->second.dwOwner);
	if (!lpc_owner)
	{
		sys_err("CompanionUpdateState: no owner %u", m_bot->second.dwOwner);
		return false;
	}
	
	// update clc general
	(*clc).i_hp = lpc_bot->GetHP();
	(*clc).i_max_hp = lpc_bot->GetMaxHP();

	(*clc).i_mp = lpc_bot->GetSP();
	(*clc).i_max_mp = lpc_bot->GetMaxSP();

	float dist_to_owner = DISTANCE_APPROX(lpc_bot->GetX() - lpc_owner->GetX(), lpc_bot->GetY() - lpc_owner->GetY());

	// Companion Logic
	// Besteht aus den folgenden drei States:
	//		- Behavior (Verhalten)
	//		- Combat (Kampf)
	//		- Movement (Bewegung)

	// Jeder der drei States hat eine gewichtung,
	// nach der die auszuf�hrende Aktion gew�hlt wird.
	// Neben den drei State Variablen, gibt es eine State Config welches mit den Values des letzten Ticks gef�llt sind.
	
	/*
		Behavior State
		Dieser State indiziert wie der Companion sich im allgemeinen Verhalten soll.
		Greift er Monster von alleine an?
		Oder nur die die den Owner angreifen.
		Oder ist der Companion im Follow Modus und ist forced nichts anzugreifen?
		All das wird hier gecheckt, und hier auf den Combat State geschrieben, der im Movement State dann den rest erledigt
	*/

	UpdateCompanionStateBehavior(get_dword_time(), m_bot);

	return true;
}
