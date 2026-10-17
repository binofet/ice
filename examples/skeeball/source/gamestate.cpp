//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "gamestate.h"
#include <chrono>

#include "skeeball.h"

// #include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.cpp"

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/input/input.h"
#include "ice/math/rand.h"

ice::math::vec3 pt_ten(-3.269999f, 20.250437f, -112.25557);
ice::math::vec3 pt_twenty(-2.799999f, 33.980305f, -124.99829f);
ice::math::vec3 pt_thirty(-2.3799999f, 45.618351f, -137.65363f);
ice::math::vec3 pt_fourty(-2.9799999f, 57.686325f, -151.56598f);
ice::math::vec3 pt_fifty(-3.139999f, 68.416206f, -165.14851f);
ice::math::vec3 pt_hundred_l(26.960522f, 80.188721f, -178.32127f);
ice::math::vec3 pt_hundred_r(-32.840496f, 80.188721f, -178.32127f);

ice::math::vec3 spawn_pos(47.823, -19.383, 114.481);

#define SQR_SCORE_DST (75.0f)

extern SkeeBall &gApp;

/*! c'tor
 **/
GameState::GameState(void) {
	content_ = NULL;

	phys_world_        = NULL;
	dispatcher_        = NULL;
	pair_cache_        = NULL;
	constraint_solver_ = NULL;
	collision_config_  = NULL;
	ticket_start_      = NULL;
	ticket_roll_       = NULL;

	bInPlay   = false;
	bWaiting  = false;
	last_lmb_ = false;
	bScored   = false;

	percent_    = 0.0f;
	dy_         = 0.0f;
	ticket_rot_ = 0.0f;
} // END FUNCTION GameState(void)

/*! d'tor
 **/
GameState::~GameState() {
	if (phys_world_)
		delete phys_world_;
	if (dispatcher_)
		delete dispatcher_;
	if (pair_cache_)
		delete pair_cache_;
	if (constraint_solver_)
		delete constraint_solver_;
	if (collision_config_)
		delete collision_config_;

} // END FUNCTION ~GameState()

/*! Initializes the games state
 *
 *  @param          pContentLoader      Pointer to main content loader
 *  @returns        ICRESULT            Success/failure/warnings of init
 **/
ice::result GameState::init(ice::assets::content_loader *pContentLoader) {
	ice::result res = IC_OK;

	if (!pContentLoader)
		return IC_FAIL_GEN;

	content_ = pContentLoader;

	ice::render::graphics_device *pDev = content_->get_device();

	(transform_) = ice::math::mat4(1);

	// get us a camera
	ice::math::real ar = (ice::math::real)gApp.get_settings().iScreen_x /
	                     (ice::math::real)gApp.get_settings().iScreen_y;
	max_cam_.get_projection() = glm::perspectiveLH_ZO(float((40.0f * IC_PI / 180.0f)), float(ar),
	                                                  float(1.0f), float(10000000.0f));

	max_cam_.zoom()     = 328.0f;
	max_cam_.zoom_max() = 400.0f;
	max_cam_.zoom_min() = 2.0f;
	max_cam_.xrot()     = 1.5584f;
	max_cam_.yrot()     = 4.683891f;
	(max_cam_.center()) = ice::math::vec3(-7.341785f, 33.721600f, -8.874823f);

	max_cam_.enable_input = gApp.get_settings().bFreeCam;

	// physics
	res |= init_physics();

	res |= score_font_.initialize("Arial", 40, content_->get_device());
	res |= font_.initialize("Arial", 24, content_->get_device());

	if (ICEFAIL(content_->load("resource/models/ticket_start.icm", &ticket_start_)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("resource/models/ticket.icm", &ticket_roll_)))
		return IC_FAIL_GEN;

	ice::math::real sx = (ice::math::real)gApp.get_settings().iScreen_x;
	ice::math::real sy = (ice::math::real)gApp.get_settings().iScreen_y;

	ice::math::real rat = sy / 1024.0f;
	ice::math::real mx  = sx / 2.0f;

	ramp_.init(content_, phys_world_);

	ice::math::vec3 pos      = spawn_pos;
	int             maxballs = ice::math::min(MAX_BALLS, MAX_BALLS_OUT);
	for (int i = 0; i < maxballs; ++i) {
		balls_[i].init(content_, phys_world_, pos);
		pos.z += 7.95f;
		pos.y -= 1.50f;
	}

	(pos) = ice::math::vec3(0.0f, -50.0f, 0.0f);
	for (int i = maxballs; i < MAX_BALLS; ++i) {
		balls_[i].init(content_, phys_world_, pos);
	}

	ice::audio::sound_param params;

	params.loop  = false;
	params.vol   = gApp.get_settings().fSfx_vol;
	params.pitch = 1.0f;

	content_->load("resource/audio/out_of_balls.wav", &loser_snd_);
	loser_snd_->set_params(params);

	content_->load("resource/audio/normpt_score.wav", &score_snd_);
	score_snd_->set_params(params);

	content_->load("resource/audio/goodpt_score.wav", &score2nd_);
	score2nd_->set_params(params);

	// m_pContent->Load("resource/models/ball.icm", &m_pTestBall);
	// m_mat.Identity();

	return res;
} // END FUNCTION Init(icContentLoader* pContentLoader)

/*! Initializes the physics
 *
 *  @returns        ICRESULT        Success/failure of physics initialization
 **/
ice::result GameState::init_physics(void) {
	collision_config_ = new btDefaultCollisionConfiguration();
	dispatcher_       = new btCollisionDispatcher(collision_config_);

	btVector3 worldMin(-1000, -1000, -1000);
	btVector3 worldMax(1000, 1000, 1000);

	pair_cache_        = new btAxisSweep3(worldMin, worldMax);
	constraint_solver_ = new btSequentialImpulseConstraintSolver();
	phys_world_        = new btDiscreteDynamicsWorld(dispatcher_, pair_cache_, constraint_solver_,
	                                                 collision_config_);

	btVector3 grav(0.0f, -30.0f, 0.0f);
	phys_world_->setGravity(grav);

	return IC_OK;
} // END FUNCTION InitPhysics(void)

/*! Updates the current scores
 **/
void GameState::update_score(const float fDeltaTime) {
	if (!phys_world_ || bScored)
		return;

	for (int i = 0; i < MAX_BALLS; ++i) {
		if (gApp.BrowserPanel && i != MAX_BALLS - cur_ball_)
			continue;
		btVector3 bv = balls_[i].body_->getWorldTransform().getOrigin();

		ice::math::vec3 pos(bv.x(), bv.y(), bv.z());

		if (glm::dot((pos) - (pt_ten), (pos) - (pt_ten)) < SQR_SCORE_DST) {
			bScored = true;
			score_ += 10;
			score_snd_->play();
		} else if (glm::dot((pos) - (pt_twenty), (pos) - (pt_twenty)) < SQR_SCORE_DST) {
			bScored = true;
			score_ += 20;
			score_snd_->play();
		} else if (glm::dot((pos) - (pt_thirty), (pos) - (pt_thirty)) < SQR_SCORE_DST) {
			bScored = true;
			score_ += 30;
			score2nd_->play();
		} else if (glm::dot((pos) - (pt_fourty), (pos) - (pt_fourty)) < SQR_SCORE_DST) {
			bScored = true;
			score_ += 40;
			score2nd_->play();
		} else if (glm::dot((pos) - (pt_fifty), (pos) - (pt_fifty)) < SQR_SCORE_DST) {
			bScored = true;
			score_ += 50;
			score2nd_->play();
		} else if (glm::dot((pos) - (pt_hundred_l), (pos) - (pt_hundred_l)) < SQR_SCORE_DST) {
			bScored = true;
			score_ += 100;
			score2nd_->play();
		} else if (glm::dot((pos) - (pt_hundred_r), (pos) - (pt_hundred_r)) < SQR_SCORE_DST) {
			bScored = true;
			score_ += 100;
			score2nd_->play();
		}
	}

	ice::math::real rot = num_tickets_ * 0.09f * 4.0f / 3.0f;
	if (ticket_rot_ != rot) {
		ticket_rot_ = ice::math::lerp(ticket_rot_, rot, fDeltaTime * 4.0f);
	}

	if (score_ > gApp.get_settings().iHighScore) {
		SkeeBallSettings settings = gApp.get_settings();
		settings.iHighScore       = score_;
		gApp.update_settings(settings);
	}

	num_tickets_ = score_ % 10 + score_ / 10;
} // END FUNCTION UpdateScore()

/*! Updates the current game that is in session
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @param          bFinished           Pointer, set to true if ste is done
 *  @returns        ICRESULT            Success/failure/warnings of update
 **/
ice::result GameState::update(const float fDeltaTime, bool *bFinished) {
	ice::result res = IC_OK;

	ice::input::input *input = ice::input::input::instance();

	bool ldown = input->is_down(ice::input::mouse_l) || input->is_down(ice::input::key_space);

#ifdef WIIMOTE
	if (gApp.GetSettings().bWiiMote) {
		wiimote &wii_remote = gApp.remote;
		if (wii_remote.IsConnected()) {
			ldown |= wii_remote.Button.B();
		}
	}
#endif

	if (gApp.BrowserPanel) {
		ldown     = false;
		last_lmb_ = false;
	}

	// Update the camera
	max_cam_.update(fDeltaTime);

	// Update the physics world
	phys_world_->stepSimulation(fDeltaTime, 8, 1.0f / 120.0f);

	Ball *ball = NULL;

	if (!bInPlay && !bWaiting && cur_ball_ < MAX_BALLS && cur_ball_ > -1) {
		ice::math::vec3 spawn = spawn_pos;
		int wrap_index        = (cur_ball_ + 2 > MAX_BALLS) ? MAX_BALLS - cur_ball_ : cur_ball_;

		if (cur_ball_ > -1 && MAX_BALLS - cur_ball_ > MAX_BALLS_OUT)
			balls_[wrap_index].set_pos(spawn);

		if (cur_ball_ < 0)
			cur_ball_ = 0;

		int index = MAX_BALLS - ++cur_ball_;
		ball      = &balls_[index];
		bWaiting  = true;

		bScored = false;

		ice::math::vec3 pos(0.0f, -23.20f, 140.0f);

		ball->set_pos(pos);

		btTransform  predictedTrans;
		btRigidBody *body = ball->body_;
		body->predictIntegratedTransform(fDeltaTime, predictedTrans);
		btScalar squareMotion =
		    (predictedTrans.getOrigin() - body->getWorldTransform().getOrigin()).length2();

		body->proceedToTransform(predictedTrans);
	} else if (bWaiting && cur_ball_ > -1) {
		ball = &balls_[MAX_BALLS - cur_ball_];

		// drop a ball for physics testing
		ice::math::mat4 mat;
		ball->get_mat(&mat);
		ice::math::vec3 pos = ice::math::vec3((mat)[3]);

		ice::math::real x = 0.0f;

#ifdef WIIMOTE
		if (gApp.GetSettings().bWiiMote) {
			wiimote &wiimote = gApp.remote;
			if (wiimote.NunchukConnected()) {
				x = wiimote.Nunchuk.Joystick.X * 100.0f * fDeltaTime;
			}
			if (wiimote.Button.Left())
				x -= 100.0f * fDeltaTime;
			if (wiimote.Button.Right())
				x += 100.0f * fDeltaTime;
		}

#endif
		x += ice::input::input::instance()->get_axis(ice::input::mouse_x) * 12000.0f * fDeltaTime;
		if (input->is_down(ice::input::key_left))
			x -= 45.0f * fDeltaTime;
		if (input->is_down(ice::input::key_right))
			x += 45.0f * fDeltaTime;
		float new_x = ice::math::clamp(pos.x + x, -31.047312f, 37.262222f);
		(pos)       = ice::math::vec3(new_x, -23.20f, 140.0f);

		if (ball && !bInPlay && bWaiting) {
			ball->set_pos(pos);
			ball->body_->activate(true);
		}
	}

	ice::input::shot_intent remoteShot;
	const double            companionNow =
	    std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	bool remote = ball && ready_for_shot() && gApp.Companion.take_shot(remoteShot, companionNow);
	if (remote && gApp.BrowserPanel) {
		companion_shooter_    = gApp.CompanionWeb.claim_shot_owner(companionNow);
		companion_shot_score_ = 0;
		companion_settled_    = false;
		if (!companion_shooter_)
			remote = false;
	}
	if (remote) {
		ice::math::vec3 position(
		    -31.047312f + (remoteShot.aim + 1) * .5f * (37.262222f + 31.047312f), -23.20f, 140.0f);
		ball->set_pos(position);
		dy_       = remoteShot.power * 1000.0f;
		ldown     = false;
		last_lmb_ = true;
	}
	if (input->is_pressed(ice::input::key_escape)) {
		render_top_only_ = true;
		ice::input::input::instance()->show_curs();
		gApp.get_state_machine().push_state(&gApp.pause_menu_);
	} else if (ball && !bInPlay && !ldown && last_lmb_) {
		ice::math::vec3 look(0.0f, -0.2f, 1.49904881f);
		(look) = glm::normalize(look);

		ice::math::real scale = ice::math::clamp(dy_, 0.0f, 1000.0f);
		if (gApp.get_settings().bWiiMote) {
			scale    = glm::length(accel_);
			(accel_) = ice::math::vec3(0.0f, 0.0f, 0.0f);
		}

		look *= -scale;
		btVector3 force(look.x, look.y, look.z);

		ball->body_->activate(true);
		ball->body_->applyImpulse(force, btVector3(0.0, 0.0, 0.0));
		if (remote)
			ice::printf("Companion launched SkeeBall player=%llu aim=%.3f power=%.3f",
			            (unsigned long long)companion_shooter_, remoteShot.aim, remoteShot.power);

		play_timer_ = 0.0f;
		bInPlay     = true;
		bWaiting    = false;

		percent_ = 0.0f;
		dy_      = 0.0f;
	}

	// check for ball status
	if (bInPlay) {
		play_timer_ += fDeltaTime;
		int index = MAX_BALLS - cur_ball_;

		if (0 <= index && index < MAX_BALLS) {
			Ball *ball = &balls_[index];

			ice::math::mat4 mat;
			ball->get_mat(&mat);

			if (ice::math::vec3((mat)[3]).y < -40.0f || play_timer_ > 5.0f) {
				bInPlay  = false;
				bWaiting = false;
				if (companion_shooter_)
					companion_settled_ = true;
			}
		}
	}

	if (ldown && last_lmb_) {
		dy_ += 650.0f * fDeltaTime;

#ifdef WIIMOTE
		if (gApp.GetSettings().bWiiMote) {
			wiimote &mote = gApp.remote;

			m_fDy += abs(mote.Acceleration.Orientation.Pitch) * 5.0f * fDeltaTime;

			m_v3Accel.x += mote.Acceleration.X * 1000.0f * fDeltaTime;
			m_v3Accel.y += mote.Acceleration.Y * 1000.0f * fDeltaTime;
			m_v3Accel.z += mote.Acceleration.Z * 1000.0f * fDeltaTime;

			m_fDy = m_v3Accel.Length();
		}
#endif

		// make sure you keep fluid movement
		dy_       = ice::math::clamp(dy_, 0.0f, 1000.0f);
		float val = ice::math::clamp(dy_, 0.0f, 1000.0f);
		percent_  = val / 1000.0f * 100.0f;
	}

	last_lmb_ = ldown;

	for (int i = 0; i < MAX_BALLS; ++i)
		balls_[i].update(fDeltaTime);

	if (!gApp.BrowserPanel && cur_ball_ == MAX_BALLS && bInPlay == false && bWaiting == false) {
		loser_snd_->play();

		cur_ball_ = -1;

		game_over_timer_ = 0.0f;
	} else if (cur_ball_ == -1) {
		game_over_timer_ += fDeltaTime;

		if (game_over_timer_ >= 6.0f) {
			ice::util::state *state = gApp.get_state_machine().pop_state();
			gApp.get_state_machine().push_state(state);
		}
	}

	// if (input->IsDown(ICKEY_W))
	//{
	//     m_mat.Translate(0.0f,0.01f,0.0f);
	// }
	// else if (input->IsDown(ICKEY_S))
	//{
	//     m_mat.Translate(0.0f,-0.01f,0.0f);
	// }
	// else if (input->IsDown(ICKEY_D))
	//{
	//     m_mat.Translate(0.01f,0.0f,0.0f);
	// }
	// else if (input->IsDown(ICKEY_A))
	//{
	//     m_mat.Translate(-0.01f,0.0f,0.0f);
	// }
	// else if (input->IsDown(ICKEY_UP))
	//{
	//     m_mat.Translate(0.0f,0.0f,0.01f);
	// }
	// else if (input->IsDown(ICKEY_DOWN))
	//{
	//     m_mat.Translate(0.0f,0.0f,-0.01f);
	// }

	int previousScore = score_;
	update_score(fDeltaTime);
	if (companion_shooter_)
		companion_shot_score_ += (unsigned)(score_ - previousScore);
	if (companion_settled_) {
		gApp.CompanionWeb.finish_shot(companion_shooter_, companion_shot_score_);
		ice::printf("Party shot settled player=%llu points=%u",
		            (unsigned long long)companion_shooter_, companion_shot_score_);
		companion_shooter_ = 0;
		companion_settled_ = false;
	}
	if (gApp.BrowserPanel && cur_ball_ == MAX_BALLS && !bInPlay && !bWaiting && !companion_shooter_)
		on_push();

	ice::audio::sound_param param;

	param.loop  = false;
	param.pitch = 1.0f;
	param.vol   = gApp.get_settings().fSfx_vol;

	score_snd_->set_params(param);
	score2nd_->set_params(param);
	loser_snd_->set_params(param);

#ifdef WIIMOTE
	if (gApp.GetSettings().bWiiMote) {
		wiimote &wii_remote = gApp.remote;
		if (wii_remote.IsConnected()) {
			if (wii_remote.Button.Home()) {
				m_bRenderTopOnly = true;
				ice::input::input::Instance()->ShowCurs();
				gApp.GetStateMachine().PushState(&gApp.m_PauseMenu);
			}
		}
	}
#endif

	return res;
} // END FUNCTION Update(const float fDeltaTime, bool *bFinished)

/*! Renders the current game that is in session
 *
 *  @returns        ICRESULT        Success/failure/warnings of render
 **/
ice::result GameState::render(void) {
	ice::result res = IC_OK;

	ice::render::graphics_device *pDev = content_->get_device();

	// A quiet stage behind the cabinets, instead of the old empty black void.
	ice::math::mat4 stage;
	(stage) = ice::math::mat4(1);
	pDev->set_projection(pDev->get_ortho());
	pDev->set_view_matrix(stage);
	pDev->set_world_transform(stage);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	int liveWidth, liveHeight;
	gApp.ui.size(liveWidth, liveHeight);
	float sw = liveWidth, sh = liveHeight;
	glBegin(GL_QUADS);
	glColor3f(.015f, .025f, .035f);
	glVertex2f(0, 0);
	glVertex2f(sw, 0);
	glColor3f(.055f, .13f, .15f);
	glVertex2f(sw, sh);
	glVertex2f(0, sh);
	glEnd();
	glPopAttrib();
	int renderWidth, renderHeight;
	gApp.ui.size(renderWidth, renderHeight);
	if (renderHeight <= 0)
		return IC_OK;
	max_cam_.get_projection() = glm::perspectiveLH_ZO(
	    float(40.f * IC_PI / 180.f), float((ice::math::real)renderWidth / renderHeight), float(1.f),
	    float(2000.f));
	// set projection and view matrix
	pDev->set_projection(max_cam_.get_projection());
	pDev->set_view_matrix(max_cam_.get_view_mat());

	res |= ramp_.render();

	for (int i = 0; i < MAX_BALLS; ++i)
		balls_[i].render();

	(m44_tickets_) = ice::math::mat4(1);
	(m44_tickets_)[3] += ice::math::vec4(ice::math::vec3(39.7f, -61.848f, 120.62f), 0); // 120.62f
	(m44_tickets_) = glm::rotate(m44_tickets_, -1.79f + ticket_rot_, ice::math::vec3(1, 0, 0));
	(m44_tickets_) = glm::rotate(m44_tickets_, IC_PI, ice::math::vec3(0, 1, 0));

	(m44_tickets_) = glm::scale(m44_tickets_, ice::math::vec3(-1.0f, 1.0f, 1.0f));

	ice::math::real rot = num_tickets_ * 0.09f * 4.0f / 3.0f;

	if (num_tickets_ < 10 || (num_tickets_ < 10 && ticket_rot_ != rot)) {
		ticket_start_->render(m44_tickets_);
	} else {
		ticket_roll_->render(m44_tickets_);
	}

	// m_pTestBall->Render(m_mat);

	pDev->set_projection(pDev->get_ortho());
	pDev->set_view_matrix(transform_);
	pDev->set_world_transform(transform_);

	int width, height;
	gApp.ui.size(width, height);
	if (!gApp.BrowserPanel) {
		gApp.ui.begin(false);
		const float x = 24, y = height - 166;
		gApp.ui.rect({x, y, 224, 142}, ice::math::color(4, 19, 27, 238));
		gApp.ui.rect({x, y + 138, 224, 4}, ice::math::color(92, 187, 174));
		gApp.ui.text(x + 18, y + 111, "SCORE", true);
		char score[24];
		std::snprintf(score, sizeof(score), "%03d", score_);
		gApp.ui.heading(x + 18, y + 61, score);
		int  balls = cur_ball_ < 0 ? 0 : MAX_BALLS - cur_ball_ + (bWaiting ? 1 : 0);
		char remaining[40];
		std::snprintf(remaining, sizeof(remaining), "BALLS LEFT   %d",
		              ice::math::clamp(balls, 0, MAX_BALLS));
		gApp.ui.text(x + 18, y + 20, remaining, true);
		if (last_lmb_) {
			char power[40];
			std::snprintf(power, sizeof(power), "POWER  %.0f%%", percent_);
			gApp.ui.rect({width - 228.f, height - 76.f, 204, 52}, ice::math::color(4, 19, 27, 238));
			gApp.ui.text(width - 210.f, height - 58.f, power);
		}
		gApp.ui.text(20, 50, "Mouse/arrows: aim  |  Hold & release: roll  |  Esc: pause", true);
		gApp.ui.end();
	}

	return res;
} // END FUNCTION Render(void)

/*! This gets called when the gamestate gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
 **/
void GameState::on_push(void) {
	companion_shooter_    = 0;
	companion_shot_score_ = 0;
	companion_settled_    = false;
	cur_ball_             = 0;
	bWaiting              = false;
	bInPlay               = false;
	bScored               = false;

	game_over_timer_ = 0.0f;

	score_ = 0;

	if (phys_world_) {
		ice::math::vec3 pos     = spawn_pos;
		int             maxball = ice::math::min(MAX_BALLS, MAX_BALLS_OUT);
		for (int i = 0; i < maxball; ++i) {
			balls_[i].set_pos(pos);
			pos.z += 7.95f;
			pos.y -= 1.50f;

			balls_[i].enable(true);
		}
		(pos) = ice::math::vec3(0.0f, -50.0f, 0.0f);
		for (int i = maxball; i < MAX_BALLS; ++i) {
			balls_[i].set_pos(pos);
			balls_[i].enable(true);
		}
	}

	ticket_rot_ = 0.0f;

	(m44_tickets_) = ice::math::mat4(1);

	// if (gApp.GetSettings().bSfx)
	//     m_score2nd->Play();

	ice::input::input::instance()->hide_curs();
} // END FUNCTION OnPush(void)

/*! This gets called when the gamestate gets popped off the stack
 *  we might not need it
 **/
void GameState::on_pop(void) {
	if (phys_world_) {
		ice::math::vec3 pos(0.0f, -130.183, 0.0f);
		for (int i = 0; i < MAX_BALLS; ++i) {
			balls_[i].set_pos(pos);

			balls_[i].enable(false);
		}
	}
} // END FUNCTION OnPop(void)