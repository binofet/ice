//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "gamestate.h"
#include "AiInput.h"
#include "NetworkInput.h"
#include "PlayerInput.h"
#include "gameplay.h"

#include "airhockey.h"

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/input/input.h"
#include "ice/math/rand.h"

ice::math::vec2 bl[] = {ice::math::vec2(0.0f, 0.0f),   ice::math::vec2(0.119f, 0.0f),
                        ice::math::vec2(0.189f, 0.0f), ice::math::vec2(0.281f, 0.0f),
                        ice::math::vec2(0.400f, 0.0f), ice::math::vec2(0.500f, 0.0f),
                        ice::math::vec2(0.600f, 0.0f), ice::math::vec2(0.700f, 0.0f),
                        ice::math::vec2(0.800f, 0.0f), ice::math::vec2(0.900f, 0.0f)};

ice::math::vec2 tl[] = {ice::math::vec2(0.0f, 1.0f),   ice::math::vec2(0.119f, 1.0f),
                        ice::math::vec2(0.189f, 1.0f), ice::math::vec2(0.281f, 1.0f),
                        ice::math::vec2(0.400f, 1.0f), ice::math::vec2(0.500f, 1.0f),
                        ice::math::vec2(0.600f, 1.0f), ice::math::vec2(0.700f, 1.0f),
                        ice::math::vec2(0.800f, 1.0f), ice::math::vec2(0.900f, 1.0f)};

ice::math::vec2 tr[] = {ice::math::vec2(0.108f, 1.0f), ice::math::vec2(0.192f, 1.0f),
                        ice::math::vec2(0.285f, 1.0f), ice::math::vec2(0.395f, 1.0f),
                        ice::math::vec2(0.500f, 1.0f), ice::math::vec2(0.600f, 1.0f),
                        ice::math::vec2(0.700f, 1.0f), ice::math::vec2(0.800f, 1.0f),
                        ice::math::vec2(0.900f, 1.0f), ice::math::vec2(1.0f, 1.0f)};

ice::math::vec2 br[] = {ice::math::vec2(0.108f, 0.0f), ice::math::vec2(0.192f, 0.0f),
                        ice::math::vec2(0.285f, 0.0f), ice::math::vec2(0.395f, 0.0f),
                        ice::math::vec2(0.500f, 0.0f), ice::math::vec2(0.600f, 0.0f),
                        ice::math::vec2(0.700f, 0.0f), ice::math::vec2(0.800f, 0.0f),
                        ice::math::vec2(0.900f, 0.0f), ice::math::vec2(1.0f, 0.0f)};

extern Airhockey &gApp;

/*! c'tor
 **/
GameState::GameState(void) {
	content_    = NULL;
	ground_tex_ = NULL;
	ground_     = NULL;

	world_   = NULL;
	GameMode = GAME_MODE_SINGLE;
	font_    = nullptr;

	wgt_win_   = NULL;
	tex_plyr1_ = NULL;
	tex_plyr2_ = NULL;

	win_timer_ = 0.0f;
} // END FUNCTION GameState(void)

/*! d'tor
 **/
GameState::~GameState() {
	if (world_)
		delete world_;
} // END FUNCTION ~GameState()

// HACK!
const ice::math::real halfx    = 500.0;
const ice::math::real halfy    = 500.0;
const int             numrow   = 50;
const int             numcol   = 50;
const float           edgeDist = (halfx * 2) / (numrow - 1);
ice::math::vec3       cam_pos(0.0f, 0.750f, -1.54f);
ice::math::vec3       look_pos(0.0f, 0.0f, 0.0f);

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
	chase_cam_.get_projection() = glm::perspectiveLH_ZO(
	    float((40.0f * IC_PI / 180.0f)), float(1280.0f / 720.0f), float(1.0f), float(10000000.0f));
	camera_.get_projection() = glm::perspectiveLH_ZO(
	    float((40.0f * IC_PI / 180.0f)), float(1280.0f / 720.0f), float(1.0f), float(10000000.0f));

	camera_.get_view_mat() =
	    ice::math::camera_pose_lh(cam_pos, look_pos, ice::math::vec3(0.0f, 1.0f, 0.0f));
	(chase_cam_.get_view_mat()) =
	    glm::translate(ice::math::mat4(1), ice::math::vec3(0.0f, 0.0f, 0.0f));
	chase_cam_.set_chase_xform(&transform_);

	res |= content_->load("resource/audio/excelent.wav", &score_snd_);
	ice::audio::sound_param param;
	param.loop  = false;
	param.pitch = 1.0f;
	param.vol   = gApp.get_settings().fSfx_vol;
	score_snd_->set_params(param);

	// create ground
	res |= create_ground();

	// physics
	res |= init_physics();

	// Initialize table
	res |= table_.init(content_, world_);

	// Initialize puck
	res |= puck_.init(content_, world_);

	// Initialize paddles
	res |= paddles_[0].init(content_, world_);
	res |= paddles_[1].init(content_, world_);

	paddles_[0].player = PLAYER_ONE;
	paddles_[1].player = PLAYER_TWO;

	ice::render::font_params fontParams = {"Arial", 36};
	res |= content_->get_device()->create_font(&font_, &fontParams);
	// res |= m_Font.Initialize("Arial",36,m_pContent);

	ice::render::texture *tex1;
	res |= content_->load("resource/textures/UI_numbers.tga", &tex1);

	res |= content_->load("resource/textures/UI_succubus_1.tga", &tex_plyr1_);
	res |= content_->load("resource/textures/UI_succubus_2.tga", &tex_plyr2_);

	ice::math::real sx = (ice::math::real)gApp.get_settings().iScreen_x;
	ice::math::real sy = (ice::math::real)gApp.get_settings().iScreen_y;

	ice::math::real rat = sy / 1024.0f;
	ice::math::real mx  = sx / 2.0f;

	// player ones score board
	wgt_score_[PLAYER_ONE] = new ice::widget(content_);
	wgt_score_[PLAYER_ONE]->set_textures(tex1);
	ice::math::vec2 bl(0.0f, 0.0f);
	ice::math::vec2 tl(0.0f, 1.0f);
	ice::math::vec2 tr(0.108f, 1.0f);
	ice::math::vec2 br(0.108f, 0.0f);
	wgt_score_[PLAYER_ONE]->set_u_vs(tl, tr, bl, br);
	wgt_score_[PLAYER_ONE]->set_size(ice::math::vec2(sy * 0.12f, sy * 0.14f));
	wgt_score_[PLAYER_ONE]->set_pos(ice::math::vec2(sx * 0.88f, sy * 0.15f));

	// player twos score board
	wgt_score_[PLAYER_TWO] = new ice::widget(content_);
	wgt_score_[PLAYER_TWO]->set_textures(tex1);
	(bl) = ice::math::vec2(0.0f, 0.0f);
	(tl) = ice::math::vec2(0.0f, 1.0f);
	(tr) = ice::math::vec2(0.108f, 1.0f);
	(br) = ice::math::vec2(0.108f, 0.0f);
	wgt_score_[PLAYER_TWO]->set_u_vs(tl, tr, bl, br);
	wgt_score_[PLAYER_TWO]->set_size(ice::math::vec2(sy * 0.12f, sy * 0.14f));
	wgt_score_[PLAYER_TWO]->set_pos(ice::math::vec2(sx * 0.78f, 0.70f));

	// winner widget
	wgt_win_ = new ice::widget(content_);
	wgt_win_->set_textures(tex_plyr1_);
	(bl) = ice::math::vec2(0.0f, 0.0f);
	(tl) = ice::math::vec2(0.0f, 1.0f);
	(tr) = ice::math::vec2(1.0f, 1.0f);
	(br) = ice::math::vec2(1.0f, 0.0f);
	wgt_win_->set_u_vs(tl, tr, bl, br);
	wgt_win_->set_size(ice::math::vec2(sy * 0.75f, sy * 0.75f));
	wgt_win_->set_pos(ice::math::vec2(mx, sy * 0.5f));

	return res;
} // END FUNCTION Init(icContentLoader* pContentLoader)

/*! Creates the ground object
 *
 *  @param          ICRESULT        Success/failure of ground creation
 **/
ice::result GameState::create_ground(void) {
	ice::result res = IC_OK;

	ice::render::graphics_device *pDev = content_->get_device();

	ice::render::vert_def vert_def;
	vert_def.num_prims = (numrow - 1) * (numcol - 1) * 2;
	vert_def.vert_size = sizeof(ice::render::vrt_dif);
	vert_def.num_verts = (numrow - 1) * (numcol - 1) * 4;
	vert_def.usage     = IC_VERT_USAGE_STATIC;
	vert_def.prim_type = ice::render::prim_tlist;
	vert_def.vert_type = ice::render::ic_vert_dif;
	res |= pDev->create_vb(&ground_, &vert_def);

	ice::render::vert_lock vertLock;
	res |= ground_->lock(&vertLock);

	ice::render::vrt_dif *vb = (ice::render::vrt_dif *)vertLock.data;

	ice::uint cur_vert = 0;

	for (int row = 0; row < numrow - 1; ++row) {
		for (int col = 0; col < numcol - 1; ++col) {
			ice::math::real x1 = (col * edgeDist) - halfx;
			ice::math::real y1 = (row * edgeDist) - halfy;
			ice::math::real x2 = ((col + 1) * edgeDist) - halfx;
			ice::math::real y2 = ((row + 1) * edgeDist) - halfy;

			(vb[cur_vert].pos)  = ice::math::vec3(x1, -10.0f, y1);
			(vb[cur_vert++].uv) = ice::math::vec2(0.0f, 0.0f);

			(vb[cur_vert].pos)  = ice::math::vec3(x1, -10.0f, y2);
			(vb[cur_vert++].uv) = ice::math::vec2(0.0f, 1.0f);

			(vb[cur_vert].pos)  = ice::math::vec3(x2, -10.0f, y2);
			(vb[cur_vert++].uv) = ice::math::vec2(1.0f, 1.0f);

			(vb[cur_vert].pos)  = ice::math::vec3(x2, -10.0f, y1);
			(vb[cur_vert++].uv) = ice::math::vec2(1.0f, 0.0f);
		}
	}

	res |= ground_->unlock();
	res |= content_->load("resource/textures/fire.tga", &ground_tex_);

	ice::render::index_def ind_def;
	ind_def.index_size = ice::render::ic_index_size_16;
	ind_def.num_index  = (numrow - 1) * (numcol - 1) * 6;
	ind_def.usage      = IC_INDEX_USAGE_STATIC;
	res |= pDev->create_ib(&ground_ib_, &ind_def);

	ice::render::index_lock iblock;
	res |= ground_ib_->lock(&iblock);

	short    *ib        = (short *)iblock.data;
	ice::uint cur_index = 0;

	for (int row = 0; row < numrow - 1; ++row) {
		for (int col = 0; col < numcol - 1; ++col) {
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4;
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4 + 1;
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4 + 2;

			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4 + 2;
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4 + 3;
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4;
		}
	}
	res |= ground_ib_->unlock();

	return res;
} // END FUNCTINO CreateGround(void)

/*! Initializes the physics
 *
 *  @returns        ICRESULT        Success/failure of physics initialization
 **/
ice::result GameState::init_physics(void) {
	b2AABB aabb;
	aabb.lowerBound.Set(-10.0f, -10.0f);
	aabb.upperBound.Set(10.0f, 10.0f);

	b2Vec2 grav(0.0f, 0.0f);
	world_ = new b2World(aabb, grav, false);

	return world_ ? IC_OK : IC_FAIL_GEN;
} // END FUNCTION InitPhysics(void)

/*! Updates the UVs on the ground plane
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @returns        ICRESULT            Success/failure of updating UVs
 **/
ice::result GameState::update_u_vs(const float fDeltaTime) {
	ice::result res = IC_OK;

	// ANIMATED UVs
	ice::render::vert_lock vlock;
	res |= ground_->lock(&vlock);

	ice::render::vrt_dif *vb = (ice::render::vrt_dif *)vlock.data;

	static float curdir = 0.0f;

	ice::math::real u1 = ice::math::cos(curdir) * 0.01f * fDeltaTime;
	ice::math::real v2 = ice::math::sin(curdir) * 0.01f * fDeltaTime;

	ice::math::real t = ice::math::rand_f(0.0001f, 0.001f);
	curdir += ice::math::rand_f(0.0f, 10000000.0f) > 500000.0f ? t : -t;

	ice::uint cur_vert = 0;
	for (int row = 0; row < numrow - 1; ++row) {
		for (int col = 0; col < numcol - 1; ++col) {
			vb[cur_vert].uv.x += u1;
			vb[cur_vert++].uv.y += v2;
			vb[cur_vert].uv.x += u1;
			vb[cur_vert++].uv.y += v2;
			vb[cur_vert].uv.x += u1;
			vb[cur_vert++].uv.y += v2;
			vb[cur_vert].uv.x += u1;
			vb[cur_vert++].uv.y += v2;
		}
	}

	res |= ground_->unlock();

	return res;
} // END FUNCTION UpdateUVs(const float fDeltaTime)

/*! Updates the current scores
 **/
void GameState::update_score() {
	if (GameMode == GAME_MODE_CLIENT) {
		for (int player = 0; player < 2; ++player) {
			int score = std::max(0, std::min(9, score_[player]));
			wgt_score_[player]->set_u_vs(tl[score], tr[score], bl[score], br[score]);
		}
		return;
	}
	if (score_[0] >= WINING_SCORE || score_[1] >= WINING_SCORE)
		return;
	b2Vec2 pos    = puck_.get_phys()->GetPosition();
	int    scorer = pos.y >= 0.87f ? PLAYER_ONE : pos.y <= -0.74f ? PLAYER_TWO : -1;
	if (scorer < 0)
		return;
	if (score_snd_ && gApp.get_settings().bSfx)
		score_snd_->play();
	++score_[scorer];
	if (score_[scorer] >= WINING_SCORE) {
		wgt_win_->set_textures(scorer == PLAYER_ONE ? tex_plyr1_ : tex_plyr2_);
		win_timer_ = WIN_STALL_TIMER;
		puck_.get_phys()->SetLinearVelocity(b2Vec2(0, 0));
	} else {
		int score = score_[scorer];
		wgt_score_[scorer]->set_u_vs(tl[score], tr[score], bl[score], br[score]);
		puck_.reset();
		paddles_[0].reset();
		paddles_[1].reset();
		serve_timer_ = 0.8f;
		// Rebase player targets after a goal instead of snapping to the old shot.
		for (auto &paddle : paddles_)
			if (auto handler = paddle.get_input_handler())
				handler->reset_target();
	}
	gApp.SendPacket.score[PLAYER_ONE] = score_[PLAYER_TWO];
	gApp.SendPacket.score[PLAYER_TWO] = score_[PLAYER_ONE];
}

/*! Updates the current game that is in session
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @param          bFinished           Pointer, set to true if ste is done
 *  @returns        ICRESULT            Success/failure/warnings of update
 **/
ice::result GameState::update(const float fDeltaTime, bool *bFinished) {
	ice::result res = IC_OK;

	// update the game objects
	res |= table_.update(fDeltaTime);

	// Update the camera
	chase_cam_.update(fDeltaTime);
	camera_.update(fDeltaTime);

	// update the physics
	if (score_[PLAYER_ONE] < WINING_SCORE && score_[PLAYER_TWO] < WINING_SCORE) {
		auto input = ice::input::input::instance();
		for (auto &paddle : paddles_)
			if (auto handler = paddle.get_input_handler())
				handler->prepare_frame(input);
		const float fixedStep = gameplay::step;
		float      &accum     = accumulator_;
		accum += std::min(fDeltaTime, 0.1f);
		if (accum >= fixedStep) {
			int numsteps = accum / fixedStep;
			for (int i = 0; i < numsteps; ++i) {
				if (serve_timer_ > 0) {
					serve_timer_ = std::max(0.0f, serve_timer_ - fixedStep);
					continue;
				}
				if (score_[0] >= WINING_SCORE || score_[1] >= WINING_SCORE)
					break;
				// Input targets are sampled once per render frame, then simulated at 120 Hz.
				res |= paddles_[0].update(fixedStep);
				res |= paddles_[1].update(fixedStep);
				res |= puck_.update(fixedStep);

				world_->Step(fixedStep, 8);
				update_score();
				++physics_steps_;
			}

			accum -= numsteps * fixedStep;
		}

		res |= update_u_vs(fDeltaTime);

		if (ice::input::input::instance()->is_pressed(ice::input::key_escape)) {
			render_top_only_ = false;
			ice::input::input::instance()->show_curs();
			gApp.get_state_machine().push_state(&gApp.pause_menu_);

			// icInput::Instance()->Update();
		}
	} else {
		win_timer_ -= fDeltaTime;
		if (win_timer_ <= 0.0f) {
			puck_.reset();
			paddles_[0].reset();
			paddles_[1].reset();
			serve_timer_       = 0.8f;
			score_[PLAYER_ONE] = 0;
			score_[PLAYER_TWO] = 0;
			for (int player = 0; player < 2; ++player)
				wgt_score_[player]->set_u_vs(tl[0], tr[0], bl[0], br[0]);
			for (auto &paddle : paddles_)
				if (auto handler = paddle.get_input_handler())
					handler->reset_target();
		}
	}
	update_score();

	if (GameMode == GAME_MODE_CLIENT) {
		score_[PLAYER_ONE] = gApp.ReceivePacket.score[PLAYER_ONE];
		score_[PLAYER_TWO] = gApp.ReceivePacket.score[PLAYER_TWO];
	}

	wgt_score_[PLAYER_ONE]->update(fDeltaTime);
	wgt_score_[PLAYER_TWO]->update(fDeltaTime);
	wgt_win_->update(fDeltaTime);

	// TODO: if score, win state and such
	return res;
} // END FUNCTION Update(const float fDeltaTime, bool *bFinished)

/*! Renders the current game that is in session
 *
 *  @returns        ICRESULT        Success/failure/warnings of render
 **/
ice::result GameState::render(void) {
	// icInput::Instance()->HideCurs();

	ice::result res = IC_OK;

	ice::render::graphics_device *pDev = content_->get_device();

	// set projection and view matrix
	pDev->set_projection(camera_.get_projection());
	pDev->set_view_matrix(camera_.get_view_mat());

	// draw the temporary ground
	pDev->set_world_transform(transform_);
	pDev->set_texture(0, ground_tex_);
	res |= pDev->draw_indexed_vert_buf(ground_, ground_ib_);

	// draw the table
	res |= table_.render();

	// draw the paddles
	res |= paddles_[0].render();
	res |= paddles_[1].render();

	// draw the puck
	res |= puck_.render();

	// pDev->EnableDepthTest(false);
	// pDev->SetCullMode(IC_CULL_OFF);
	pDev->set_projection(pDev->get_ortho());
	pDev->set_view_matrix(transform_);
	// pDev->SetWorldTransform(m_Transform);

	// icColor b = icColor::BLUE;
	// int score = 0;
	//// WTF FONTS
	// m_Font.Printf(0,0,b,"Player 1: %d", score);
	// m_Font.Printf(1000,100,b,"Player 2: %d", score);
	// pDev->EnableDepthTest(true);

	wgt_score_[PLAYER_ONE]->render();
	wgt_score_[PLAYER_TWO]->render();

	ice::math::real sx = (ice::math::real)gApp.get_settings().iScreen_x;
	ice::math::real sy = (ice::math::real)gApp.get_settings().iScreen_y;

	ice::math::real rat = sy / 1024.0f;
	ice::math::real mx  = sx / 2.0f;

	wgt_score_[PLAYER_ONE]->set_pos(ice::math::vec2(sx * 0.85f, sy * 0.20f));
	wgt_score_[PLAYER_TWO]->set_pos(ice::math::vec2(sx * 0.57f, sy * 0.84f));

	if (font_) {
		font_->print(ice::math::ivec2(24, (int)sy - 40), ice::math::color::white,
		             GameMode == GAME_MODE_SINGLE
		                 ? "Mouse: your paddle   |   Opponent: CPU   |   Esc: pause"
		                 : "Mouse: player 1   |   Arrow keys: player 2   |   Esc: pause");
		if (serve_timer_ > 0)
			font_->print(ice::math::ivec2((int)(sx * 0.43f), (int)(sy * 0.50f)),
			             ice::math::color::white, "GET READY");
	}
	if (score_[PLAYER_ONE] >= WINING_SCORE || score_[PLAYER_TWO] >= WINING_SCORE) {
		pDev->set_projection(pDev->get_ortho());
		ice::math::mat4 temp;
		(temp) = ice::math::mat4(1);
		pDev->set_view_matrix(temp);
		pDev->set_world_transform(temp);
		wgt_win_->render();
	}

	return res;
} // END FUNCTION Render(void)

/*! This gets called when the gamestate gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
 **/
void GameState::on_push(void) {
	accumulator_   = 0;
	physics_steps_ = 0;
	serve_timer_   = 0.8f;
	win_timer_     = 0;
	// reset physics
	puck_.reset();
	paddles_[0].reset();
	paddles_[1].reset();

	// setup input handlers
	InputHandler *p0 = paddles_[0].get_input_handler();
	InputHandler *p1 = paddles_[1].get_input_handler();

	if (p0)
		delete p0;
	if (p1)
		delete p1;

	switch (GameMode) {
	case GAME_MODE_SINGLE:
		p0 = new PlayerInput(PLAYER_ONE, puck_.get_phys(), paddles_[0].get_phys());
		p1 = new AiInput(PLAYER_TWO, puck_.get_phys(), paddles_[1].get_phys());
		break;
	case GAME_MODE_TWO:
		p0 = new PlayerInput(PLAYER_ONE, puck_.get_phys(), paddles_[0].get_phys());
		p1 = new PlayerInput(PLAYER_TWO, puck_.get_phys(), paddles_[1].get_phys());
		break;
	case GAME_MODE_HOST:
	case GAME_MODE_CLIENT:
		p0 = new PlayerInput(PLAYER_ONE, puck_.get_phys(), paddles_[0].get_phys());
		p1 = new NetworkInput(PLAYER_TWO, puck_.get_phys(), paddles_[1].get_phys());
		break;
	default:
		p0 = new PlayerInput(PLAYER_ONE, puck_.get_phys(), paddles_[0].get_phys());
		p1 = new PlayerInput(PLAYER_TWO, puck_.get_phys(), paddles_[1].get_phys());
		ice::warning("invalid game mode entered!");
	}

	paddles_[0].set_input_handler(p0);
	paddles_[1].set_input_handler(p1);

	score_[PLAYER_ONE] = 0;
	score_[PLAYER_TWO] = 0;

	if (wgt_score_[PLAYER_TWO])
		wgt_score_[PLAYER_TWO]->set_u_vs(tl[0], tr[0], bl[0], br[0]);
	if (wgt_score_[PLAYER_ONE])
		wgt_score_[PLAYER_ONE]->set_u_vs(tl[0], tr[0], bl[0], br[0]);

	ice::input::input::instance()->hide_curs();
} // END FUNCTION OnPush(void)

/*! This gets called when the gamestate gets popped off the stack
 *  we might not need it
 **/
void GameState::on_pop(void) {
	ice::input::input::instance()->show_curs();
} // END FUNCTION OnPop(void)