/**
CAST 3
alignment.h
Purpose:
Specific algorithms for rotational and translational alignment


@author Dustin Kaiser
@version 1.0
*/

#pragma once
#include "matop.h"

namespace align
{
	/**
	* Returns rotated coords-obj according to reference coords-obj "reference"
	* using Kabsch's-Algorithm
	*
	* @centerOfMassAlignment: Should center-of-mass be aligned prior to
	* rotational alignment? default = true
	*/
	coords::Coordinates kabschAligned(coords::Coordinates const& input, coords::Coordinates const& reference, bool centerOfMassAlign = true);

	/**
	* rotates coords-obj according to reference coords-obj "ref"
	* using Kabsch's-Algorithm
	*
	* @param centerOfMassAlignment: Should center-of-mass be aligned prior to rotational alignment? default = true
	* (this calls "centerOfMassAlignment" function before actual alignment procedure)
	*/
	void kabschAlignment(coords::Coordinates& input, coords::Coordinates const& reference, bool centerOfMassAlign = true);

	/**
	* Aligns mathmatrix-obj's center of mass to origin of coordinate system
	* ie.: translational alignment
	*/
	void centerOfMassAlignment(coords::Coordinates& in);

	/**
	* Returns dRMSD-value of the structure in respect to the reference structure.
	*
	* @param input: Input Structure
	* @param ref: Reference Structure
	*/
	float_type drmsd_calc(coords::Coordinates const& input, coords::Coordinates const& ref);

	/**
	* Calculates Holm&Sanders Distance of the structures
	*
	* @param input: Input Structure
	* @param ref: Reference Structure
	* @param holAndSanderDistance: Holm & Sander Distance, unique parameter for this distance metric, see original publication
	*
	*/
	float_type holmsander_calc(coords::Coordinates const& input, coords::Coordinates const& ref, double holmAndSanderDistance = 20);
}


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//                    W R A P P E R F U N C T I O N                   //
////////////////////////////////////////////////////////////////////////


void alignment(std::unique_ptr<coords::input::format>& ci, coords::Coordinates& coords);
