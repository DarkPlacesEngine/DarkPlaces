float lastclientthink, sv_maxspeed, sv_friction, sv_accelerate, sv_stopspeed;
float sv_edgefriction, cl_rollspeed, cl_divspeed;

// LordHavoc:
// Highly optimized port of SV_ClientThink from engine code to QuakeC.
// No behavior changes!  This code is much shorter and probably faster than
// the engine code :)

// note that darkplaces engine will call this function if it finds it,
// so modify for your own mods and enjoy...

// note also, this code uses some builtin functions from dpextensions.qc
// (included with darkplaces engine releases)

// P.S. if you find something weird in this code, it's just mimicing weird
// stuff in the original quake engine code (which was so unreadable it was
// hard to even identify what it was doing)

void() SV_PlayerPhysics =
{
	local vector wishvel, wishdir, v;
	local float wishspeed, f, limit;

	if (self.movetype == MOVETYPE_NONE)
		return;

	if (self.punchangle != '0 0 0')
	{
		f = vlen(self.punchangle) - 10 * frametime;
		if (f > 0)
			self.punchangle = normalize(self.punchangle) * f;
		else
			self.punchangle = '0 0 0';
	}

	// if dead, behave differently
	if (self.health <= 0)
		return;

	if (time != lastclientthink)
	{
		lastclientthink = time;
		sv_maxspeed = cvar("sv_maxspeed");
		sv_friction = cvar("sv_friction");
		sv_accelerate = cvar("sv_accelerate");
		sv_stopspeed = cvar("sv_stopspeed");
		sv_edgefriction = cvar("edgefriction");
		// LordHavoc: this * 4 is an optimization
		cl_rollangle = cvar("cl_rollangle") * 4;
		// LordHavoc: this 1 / is an optimization
		cl_divspeed = 1 / cvar("cl_rollspeed");
	}

	// show 1/3 the pitch angle and all the roll angle
	self.angles_z = bound(-1, self.velocity * v_right * cl_divspeed, 1) * cl_rollangle;
	if (!self.fixangle)
	{
		self.angles_x = (self.v_angle_x + self.punchangle_x) * -0.333;
		self.angles_y = self.v_angle_y + self.punchangle_y;
	}

	if (self.flags & FL_WATERJUMP )
	{
		self.velocity_x = self.movedir_x;
		self.velocity_y = self.movedir_y;
		if (time > self.teleport_time || self.waterlevel == 0)
		{
			self.flags = self.flags - (self.flags & FL_WATERJUMP);
			self.teleport_time = 0;
		}
		return;
	}

	// swim
	if (self.waterlevel >= 2)
	if (self.movetype != MOVETYPE_NOCLIP)
	{
		makevectors(self.v_angle);
		if (self.movement == '0 0 0')
			wishvel = '0 0 -60'; // drift towards bottom
		else
			wishvel = v_forward * self.movement_x + v_right * self.movement_y + '0 0 1' * self.movement_z;

		wishspeed = vlen(wishvel);
		if (wishspeed > sv_maxspeed)
			wishspeed = sv_maxspeed * 0.7;
		else
			wishspeed = wishspeed * 0.7;

		// water friction
		if (self.velocity != '0 0 0')
		{
			f = vlen(self.velocity) * (1 - frametime * sv_friction);
			if (f > 0)
				self.velocity = normalize(self.velocity) * f;
			else
				self.velocity = '0 0 0';
		}
		else
			f = 0;

		// water acceleration
		if (wishspeed <= f)
			return;

		limit = sv_accelerate * wishspeed * frametime;
		f = wishspeed - f;
		if (f > limit)
			self.velocity = self.velocity + normalize(wishvel) * limit;
		else
			self.velocity = self.velocity + normalize(wishvel) * f;
		return;
	}

	if (self.movetype == MOVETYPE_FLY)
		makevectors(self.v_angle);
	else
		makevectors(self.v_angle_y * '0 1 0');

	// hack to not let you back into teleporter
	wishvel = v_right * self.movement_y;
	if (time >= self.teleport_time || self.movement_x > 0)
		wishvel = wishvel + v_forward * self.movement_x;
	if (self.movetype != MOVETYPE_WALK)
		wishvel_z = wishvel_z + self.movement_z;

	wishdir = normalize(wishvel);
	wishspeed = vlen(wishvel);
	if (wishspeed > sv_maxspeed)
		wishspeed = sv_maxspeed;

	if (self.movetype == MOVETYPE_NOCLIP || self.movetype == MOVETYPE_FLY)
	{
		self.velocity = wishdir * wishspeed;
		return;
	}

	if (self.flags & FL_ONGROUND) // walking
	{
		// friction
		if (self.velocity_x || self.velocity_y)
		{
			v = self.velocity;
			v_z = 0;
			f = vlen(v);

			// if the leading edge is over a dropoff, increase friction
			v = self.origin + normalize(v) * 16 + '0 0 1' * self.mins_z;

			traceline(v, v + '0 0 -34', TRUE, self);

			// apply friction
			if (trace_fraction == 1.0)
			{
				if (f < sv_stopspeed)
					f = 1 - frametime * (sv_stopspeed / f) * sv_friction * sv_edgefriction;
				else
					f = 1 - frametime * sv_friction * sv_edgefriction;
			}
			else
			{
				if (f < sv_stopspeed)
					f = 1 - frametime * (sv_stopspeed / f) * sv_friction;
				else
					f = 1 - frametime * sv_friction;
			}

			if (f < 0)
				self.velocity = '0 0 0';
			else
				self.velocity = self.velocity * f;
		}
	}
	else // airborn
		if (wishspeed > 30)
			wishspeed = 30;

	// acceleration
	f = wishspeed - (self.velocity * wishdir);
	if (f > 0)
	{
		limit = sv_accelerate * frametime * wishspeed;
		if (f > limit)
			f = limit;
		self.velocity = self.velocity + wishdir * f;
	}
}
