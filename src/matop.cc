// For more information see matop.h

#include "matop.h"
namespace matop
{
  /////////////////////////////////////
  //                                ///
  //  S P E C I F I C   T A S K S   ///
  //                                ///
  /////////////////////////////////////

  Matrix_Class transfer_to_matr(coords::Coordinates const& in)
  {
    Matrix_Class out_mat(in.size(), 3u);
    for (size_t l = 0; l < in.size(); l++)
    {
      coords::cartesian_type tempcoord2;
      tempcoord2 = in.xyz(l);
      out_mat(l, 0) = tempcoord2.x();
      out_mat(l, 1) = tempcoord2.y();
      out_mat(l, 2) = tempcoord2.z();
    }
    return transposed(out_mat);
  }

  Matrix_Class transfer_to_matr_internal(coords::Coordinates const& in)
  {
    const size_t sizer = in.size();
    Matrix_Class out_mat(sizer, 3u);
    for (size_t l = 0; l < in.size(); l++)
    {
      out_mat(l, 0) = in.intern(l).radius();
      out_mat(l, 1) = in.intern(l).inclination().radians();
      out_mat(l, 2) = in.intern(l).azimuth().radians();
    }
    return transposed(out_mat);
  }

  Matrix_Class transform_coordinates(coords::Coordinates& input)
  {
	  Matrix_Class output(3, input.size());
	  for (size_t l = 0; l < input.size(); l++)
	  {
		  (output)((l), 0) = input.xyz(l).x();
		  (output)((l), 1) = input.xyz(l).y();
		  (output)((l), 2) = input.xyz(l).z();
	  }
    return output;
  }

  coords::Representation_3D transfer_to_3DRepressentation(Matrix_Class const& input)
  {
    coords::Representation_3D tempcoord1;

    for (size_t i = 0; i < input.cols(); i++)
    {
      coords::Cartesian_Point tempcoord2(input(0, i), input(1, i), input(2, i));
      tempcoord1.push_back(tempcoord2);
    }
    return tempcoord1;
  }

  coords::Representation_Internal transfer_to_internalRepressentation(Matrix_Class const& input)
  {
    coords::Representation_Internal tempcoord1;

    for (size_t i = 0; i < input.cols(); i++)
    {
      coords::internal_type tempcoord2(input(0, i), scon::ang<float_type>(input(1, i)), scon::ang<float_type>(input(2, i)));
      tempcoord1.push_back(tempcoord2);
    }
    return tempcoord1;
  }

  void massweight(Matrix_Class& input, coords::Coordinates const& coords, bool to_meter, std::vector<size_t> atomsThatAreUsed)
  //coords are reference coord object to get atomic masses from
  //boolean controls whether coords should also be multiplied with 10e-10 to convert angstrom to meters)
  {
    if (atomsThatAreUsed.empty())
    {
      for (size_t i = 0; i < input.rows(); i = i + 3)
      {
        double temp = sqrt(coords.atoms(i / 3u).mass() * 1.6605402 * 10e-27);
        if (to_meter)
        {
          temp *= 10e-10;
        }
        for (size_t j = 0; j < input.cols(); j++)
        {
          for (size_t k = 0; k < 3; k++)
          {
            (input)(i + k, j) *= temp;
          }
        }
      }
    }
    else
    {
      for (size_t i = 0; i < input.rows(); i = i + 3)
      {
        double temp = sqrt(coords.atoms(atomsThatAreUsed[i / 3u]).mass() * 1.6605402 * 10e-27);
        if (to_meter)
        {
          temp *= 10e-10;
        }
        for (size_t j = 0; j < input.cols(); j++)
        {
          for (size_t k = 0; k < 3; k++)
          {
            (input)(i + k, j) *= temp;
          }
        }
      }
    }
  }

  void undoMassweight(Matrix_Class& input, coords::Coordinates const& coords, bool to_meter, std::vector<size_t> atomsThatAreUsed)
  {
    if (atomsThatAreUsed.empty())
    {
      for (size_t i = 0; i < input.rows(); i = i + 3)
      {
        double temp = sqrt(coords.atoms(i / 3u).mass() * 1.6605402 * 10e-27);
        if (to_meter)
        {
          temp *= 10e-10;
        }
        for (size_t j = 0; j < input.cols(); j++)
        {
          for (size_t k = 0; k < 3; k++)
          {
            (input)(i + k, j) /= temp;
          }
        }
      }
    }
    else
    {
      for (size_t i = 0; i < input.rows(); i = i + 3)
      {
        double temp = sqrt(coords.atoms(atomsThatAreUsed[i / 3u]).mass() * 1.6605402 * 10e-27);
        if (to_meter)
        {
          temp *= 10e-10;
        }
        for (size_t j = 0; j < input.cols(); j++)
        {
          for (size_t k = 0; k < 3; k++)
          {
            (input)(i + k, j) /= temp;
          }
        }
      }
    }
  }

  Matrix_Class transformToOneline(coords::Coordinates const& coords, std::vector<size_t> const& includedAtoms, bool internalCoordinates)
  {
    //First, some range checks
    if (/*if not all atoms*/ includedAtoms.size() != 0 && includedAtoms[includedAtoms.size() - 1u] > coords.atoms().size())
    {
      throw std::runtime_error("Truncation number is greater than the total number of atoms.");
    }
    else if (internalCoordinates && includedAtoms[0] < 3u)
    {
      throw std::runtime_error("Dihedral with index < 3 specified.");
    }

    if (internalCoordinates)
    {
      // Matrix Size
      Matrix_Class transformed_matrix(1u, (includedAtoms.size() * 2));

      size_t j = 0;
      size_t quicksearch_dih = 0;
      for (size_t i = 0; i < coords.atoms().size(); i++)
      {
        size_t keeper = 0;
        bool checker_dih = false;
        for (size_t l = quicksearch_dih; l < Config::get().PCA.pca_internal_dih.size(); l++)
        {
          if (Config::get().PCA.pca_internal_dih.size() != 0)
          {
            if (Config::get().PCA.pca_internal_dih[l] == i)
            {
              checker_dih = true;
              quicksearch_dih++;
              break;
            }
          }
        }
        if (checker_dih)
        {
          transformed_matrix(0, j + keeper) = cos(coords.intern(i).azimuth().radians());
          transformed_matrix(0, j + keeper + 1) = sin(coords.intern(i).azimuth().radians());
          keeper += 2;
        }
        j = j + keeper;
      }
      return transformed_matrix;
    }
    else
    {
      if (includedAtoms.size() != 0u)
      {
        Matrix_Class transformed_matrix;
        transformed_matrix = Matrix_Class(1u, (3 * includedAtoms.size()));

        int j = 0;
        size_t quicksearch = 0;
        for (size_t i = 0; i < coords.atoms().size(); i++) //iterate over atoms
        {
          bool checker = false;
          for (size_t l = quicksearch; l < includedAtoms.size(); l++) //iterate over vector of atoms to account for
          {
            if (includedAtoms[l] - 1 == i)
            {
              checker = true;
              quicksearch++;
              break;
            }
          }
          if (checker)
          {
            transformed_matrix(0, j + 0u) = coords.xyz(i).x();
            transformed_matrix(0, j + 1u) = coords.xyz(i).y();
            transformed_matrix(0, j + 2u) = coords.xyz(i).z();
            j = j + 3u;
          }
        }
        return transformed_matrix;
      }
      else
      {
          Matrix_Class transformed_matrix(1, (coords.atoms().size() * 3u));
          int j = 0;
          for (size_t i = 0; i < coords.atoms().size(); i++)
          {
            for (size_t k = 0; k < 3; k++)
            {
              //transformed_matrix(0, j + k) = input(k, i);
              transformed_matrix(0, j + 0u) = coords.xyz(i).x();
              transformed_matrix(0, j + 1u) = coords.xyz(i).y();
              transformed_matrix(0, j + 2u) = coords.xyz(i).z();
            }
            j = j + 3u;
          }
          return transformed_matrix;
      }
    }
  }

  /////////////////////////////////////
  //                              /////
  //    E X C L U S I V E L Y     /////
  //            P C A             /////
  //                              /////
  /////////////////////////////////////
  namespace pca
  {
    void readEigenvectorsAndModes(Matrix_Class& eigenvectors, Matrix_Class& trajectory, std::string& additionalInformation, std::string filename)
    {
      std::ifstream pca_modes_stream(filename, std::ios::in);
      std::string line;
      std::getline(pca_modes_stream, line);
      //int dimensions = std::stoi(line.substr(13, 2));
      while (line.find("Eigenvectors") == std::string::npos)
      {
        std::getline(pca_modes_stream, line);
      }

      std::getline(pca_modes_stream, line);
      eigenvectors.resize(stoi(line.substr(7, 10)), stoi(line.substr(20, 10)));

      for (size_t i = 0u; i < eigenvectors.rows(); i++)
      {
        std::getline(pca_modes_stream, line);
        size_t whitespace = 0u, lastWhitespace = 0u;
        for (size_t j = 0u; j < eigenvectors.cols(); j++)
        {
          lastWhitespace = whitespace;
          whitespace = line.find(" ", lastWhitespace + 1u);

          eigenvectors(i, j) = stod(line.substr(lastWhitespace, whitespace - lastWhitespace));
        }
      }
      std::getline(pca_modes_stream, line);
      std::getline(pca_modes_stream, line);
      std::getline(pca_modes_stream, line);
      std::getline(pca_modes_stream, line);
      std::getline(pca_modes_stream, line);
      trajectory.resize(stoi(line.substr(7, 10)), stoi(line.substr(20, 10)));

      for (size_t i = 0u; i < trajectory.rows(); i++)
      {
        std::getline(pca_modes_stream, line);
        size_t whitespace = 0u, lastWhitespace = 0u;
        for (size_t j = 0u; j < trajectory.cols(); j++)
        {
          lastWhitespace = whitespace;
          whitespace = line.find(" ", lastWhitespace + 1u);

          trajectory(i, j) = stod(line.substr(lastWhitespace, whitespace - lastWhitespace));
        }
      }
      //Additional options following:
      // By the way, the rest of the file is ignored
      if (std::getline(pca_modes_stream, line))
      {
        std::getline(pca_modes_stream, line);
        if (std::getline(pca_modes_stream, line))
        {
          additionalInformation = line;
        }
        else
        {
          std::cout << "Could not read additional Information from pca_modes file.\n";
        }
      }
    }
  }


  /////////////////////////////////////
  //                              /////
  //    E X C L U S I V E L Y     /////
  //        E N T R O P Y         /////
  //                              /////
  /////////////////////////////////////
  namespace entropy
  {

    float_type knn_distance(Matrix_Class const& input, size_t const& dimension_in, size_t const& k_in, size_t const& row_queryPt, size_t const& col_queryPt, coords::float_type* buffer)
    {
      float_type temp_distance = 0.0;
      float_type hold_distance;
      float_type *distanceList = buffer;
      if (buffer == nullptr)
      {
        distanceList = new float_type[k_in];
      }
      distanceList[0] = std::numeric_limits<float_type>::max();
      // This iterates over the "n-th" next neighbors
      // (to get the second next neighbor you have to find the first next neighbor etc. )
      for (size_t i = 0; i < k_in; i++)
      {
        // Get max() is initial value for distance comparison.
        // This garantues that the first calcualted value is smaller than
        // hold_distance.
        hold_distance = std::numeric_limits<float_type>::max();

        // This iterates over all points in the set
        for (size_t j = 0; j < input.cols(); j++)
        {
          // Of course we cannot count the distance of an member to itself
          // since it is =0.0
          if (j == col_queryPt) { continue; }

          // For number of dimensions, add the squared distance of the queryPt
          // to the current point ("j") of each dimensions which equals a
          // squared distance in euclidean space
          temp_distance = 0.0;
          for (size_t l = 0; l < dimension_in; l++)
          {
            temp_distance += pow((input)(row_queryPt + l, j) - (input)(row_queryPt + l, col_queryPt), 2);
          }

          // If we are searching for the actual, "first" nearest neighbor
          // we will compare the distance in "hold_distance", initialized as a huge number,
          // to the calcualted temp_distance ("Is this point nearer to the query point
          // than the previously established nearest point?"). We will, of course,
          // keep the smaller (squared) distance of the two.
          if (i == 0 && temp_distance < hold_distance)
          {
            hold_distance = temp_distance;
          }

          // If we are searching for the "(i + 1)-th" nearest neighbot
          // we will compare "hold_distance" to "temp_distance". If temp_distance is smaller, it
          // will be kept, however, only if it is also larger than the previously established
          // "i-th" nearest neighbor. Otherwise we would always just get the absolute nearest neighbor in
          // a set of points, and not the k-th nearest neighbor.
          else if (i > 0 && temp_distance < hold_distance && temp_distance > distanceList[i - 1])
          {
            hold_distance = temp_distance;
          }
        }
        distanceList[i] = hold_distance;
      }
      float_type keeper = distanceList[k_in - 1u];
      if (buffer == nullptr) delete[] distanceList;
      return keeper;
    }

    float_type knn_distance(Matrix_Class const& input, size_t const& dimension_in, size_t const& k_in, std::vector<size_t>& row_queryPts, size_t const& col_queryPt, coords::float_type* buffer)
    //Returns squared distances in the higher-dimensional NN-query case. Needs vector-form input of query Pts
    //Will throw if input is wrong
    {
      //CHECKS
      if (row_queryPts.size() != dimension_in)
      {
        throw("Error in Matrix_Class-function \"knn_distance\": size of \'row_queryPts\' and \'dimension_in\' does not match.");
      }

      float_type temp_distance;
      float_type hold_distance;
      float_type *distanceList = buffer;
      if (buffer == nullptr)
      {
        distanceList = new float_type[k_in];
      }
      distanceList[0] = std::numeric_limits<double>::max();

      // This iterates over the "n-th" next neighbors
      // (to get the second next neighbor you have to find the first next neighbor etc. )
      for (size_t i = 0; i < k_in; i++)
      {
        // Get max() is initial value for distance comparison.
        // This garantues that the first calcualted value is smaller than
        // hold_distance.
        hold_distance = std::numeric_limits<float_type>::max();

        for (size_t j = 0; j < input.cols(); j++)
        {
          // Of course we cannot count the distance of an member to itself
          // since it is =0.0
          if (j == col_queryPt) { continue; }

          // For number of dimensions, add the squared distance of the queryPt
          // to the current point ("j") of each dimensions which equals a
          // squared distance in euclidean space
          temp_distance = 0.0;
          for (size_t l = 0; l < dimension_in; l++)
          {
            temp_distance += pow((input)(row_queryPts[l], j) - (input)(row_queryPts[l], col_queryPt), 2);
          }

          // If we are searching for the actual, "first" nearest neighbor
          // we will compare the distance in "hold_distance", initialized as a huge number,
          // to the calcualted temp_distance ("Is this point nearer to the query point
          // than the previously established nearest point?"). We will, of course,
          // keep the smaller (squared) distance of the two.
          if (i == 0 && temp_distance < hold_distance)
          {
            hold_distance = temp_distance;
          }

          // If we are searching for the "(i + 1)-th" nearest neighbot
          // we will compare "hold_distance" to "temp_distance". If temp_distance is smaller, it
          // will be kept, however, only if it is also larger than the previously established
          // "i-th" nearest neighbor. Otherwise we would always just get the absolute nearest neighbor in
          // a set of points, and not the k-th nearest neighbor.
          else if (i > 0 && temp_distance < hold_distance && temp_distance > distanceList[i - 1])
          {
            hold_distance = temp_distance;
          }
        }
        distanceList[i] = hold_distance;
      }
      float_type keeper = distanceList[k_in - 1u];
      if (buffer == nullptr) delete[] distanceList;
      return keeper;
    }
  }

  /////////////////////////////////////
  //                              /////
  //    E X C L U S I V E L Y     /////
  //      A L I G N M E N T       /////
  //                              /////
  /////////////////////////////////////
  namespace align
  {
    float_type drmsd_calc(coords::Coordinates const& input, coords::Coordinates const& ref)
    {
      if (input.atoms().size() != ref.atoms().size()) throw std::logic_error("Number of atoms of structures passed to drmsd_calc to not match.");

      float_type value = 0;
      for (size_t i = 0; i < input.atoms().size(); i++)
      {
        for (size_t j = 0; j < i; j++)
        {
          float_type holder = sqrt((ref.xyz(i).x() - ref.xyz(j).x()) * (ref.xyz(i).x() - ref.xyz(j).x()) + (ref.xyz(i).y() - ref.xyz(j).y())* (ref.xyz(i).y() - ref.xyz(j).y()) + (ref.xyz(i).z() - ref.xyz(j).z()) * (ref.xyz(i).z() - ref.xyz(j).z()));
          float_type holder2 = sqrt((input.xyz(i).x() - input.xyz(j).x()) * (input.xyz(i).x() - input.xyz(j).x()) + (input.xyz(i).y() - input.xyz(j).y())* (input.xyz(i).y() - input.xyz(j).y()) + (input.xyz(i).z() - input.xyz(j).z()) * (input.xyz(i).z() - input.xyz(j).z()));
          value += (holder2 - holder) * (holder2 - holder);
        }
      }
      return sqrt(value / (double) (input.atoms().size() * (input.atoms().size() + 1u) ) );
    }

    float_type holmsander_calc(coords::Coordinates const& input, coords::Coordinates const& ref, double holmAndSanderDistance)
    {
      if (input.atoms().size() != ref.atoms().size()) throw std::logic_error("Number of atoms of structures passed to drmsd_calc to not match.");

      float_type value(0);
      for (size_t i = 0; i < input.atoms().size(); i++) {
        for (size_t j = 0; j < i; j++)
        {
          float_type holder = sqrt((ref.xyz(i).x() - ref.xyz(j).x()) * (ref.xyz(i).x() - ref.xyz(j).x()) + (ref.xyz(i).y() - ref.xyz(j).y())* (ref.xyz(i).y() - ref.xyz(j).y()) + (ref.xyz(i).z() - ref.xyz(j).z()) * (ref.xyz(i).z() - ref.xyz(j).z()));
          float_type holder2 = sqrt((input.xyz(i).x() - input.xyz(j).x()) * (input.xyz(i).x() - input.xyz(j).x()) + (input.xyz(i).y() - input.xyz(j).y())* (input.xyz(i).y() - input.xyz(j).y()) + (input.xyz(i).z() - input.xyz(j).z()) * (input.xyz(i).z() - input.xyz(j).z()));
          value += abs(holder2 - holder) * exp(-1 * (holder2 + holder)*(holder2 + holder) / (4 * holmAndSanderDistance * holmAndSanderDistance)) / (holder2 + holder);
        }
      }
      return value;
    }

    coords::Coordinates kabschAligned(coords::Coordinates const& inputCoords, coords::Coordinates const& reference, bool centerOfMassAlign)
    {
      coords::Coordinates output(inputCoords);
      if (centerOfMassAlign) centerOfMassAlignment(output);
      kabschAlignment(output, reference);
      return output;
    }

    void kabschAlignment(coords::Coordinates& inputCoords, coords::Coordinates const& reference, bool centerOfMassAlign)
    {
      if (centerOfMassAlign)
      {
        centerOfMassAlignment(inputCoords);
      }

      Matrix_Class input = transfer_to_matr(inputCoords);
      Matrix_Class ref = transfer_to_matr(reference);

      Matrix_Class c(input * transposed(ref));
      //Creates Covariance Matrix

      Matrix_Class s, V, U;
      c.singular_value_decomposition(U, s, V);

      Matrix_Class unit = Matrix_Class::identity(c.rows(), c.rows());
      if ((c.det_sign() < 0)) //Making sure that U will do a proper rotation (rows/columns have to be right handed system)
      {
        unit(2, 2) = -1;
      }
      transpose(U);
      unit = unit * U;
      unit = V * unit;
      input = unit * input;

      inputCoords.set_xyz(transfer_to_3DRepressentation(input));
    }

    void centerOfMassAlignment(coords::Coordinates& coords_in)
    {
      coords::Cartesian_Point com_ref = coords_in.center_of_mass();
      coords_in.move_all_by(-com_ref, true);
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                    W R A P P E R F U N C T I O N S                         //
////////////////////////////////////////////////////////////////////////////////

void alignment(std::unique_ptr<coords::input::format>& ci, coords::Coordinates& coords)
{
  using namespace matop;
  using namespace matop::align;

  coords::Coordinates coordsReferenceStructure(coords), coordsTemporaryStructure(coords);

  // Check if reference structure is in range
  if (Config::get().alignment.reference_frame_num >= ci->size()) throw std::runtime_error("Reference frame number in ALIGN task is bigger than number of frames in the input structure ensemble.");

  auto temporaryPESpoint = ci->PES()[Config::get().alignment.reference_frame_num].structure.cartesian;

  //Alignment to external reference frame (different file)
  if (!Config::get().alignment.align_external_file.empty())
  {
    std::unique_ptr<coords::input::format> externalReferenceStructurePtr(coords::input::new_format());
    coords::Coordinates externalReferenceStructure(externalReferenceStructurePtr->read(Config::get().alignment.align_external_file));
    if (Config::get().alignment.reference_frame_num >= externalReferenceStructurePtr->PES().size())
    {
      throw std::out_of_range("Requested reference frame number not within reference structure ensemble.");
    }
    temporaryPESpoint = externalReferenceStructurePtr->PES()[Config::get().alignment.reference_frame_num].structure.cartesian;
  }
  //Constructs two coordinate objects and sets reference frame according to INPUTFILE
  coordsReferenceStructure.set_xyz(temporaryPESpoint);

  //Construct and Allocate arrays for stringoutput (necessary for OpenMP)
  double mean_value = 0;
  std::string *hold_str, *hold_coords_str;
  hold_str = new std::string[ci->size()];
  hold_coords_str = new std::string[ci->size()];

  //Perform translational alignment for reference frame
  if (Config::get().alignment.traj_align_translational)
  {
    centerOfMassAlignment(coordsReferenceStructure);
  }

  // Output text
  if (Config::get().general.verbosity > 2U) std::cout << "ALIGN preparations done. Starting actual alignment.\n";

#ifdef _OPENMP
  if (Config::get().general.verbosity > 3U) std::cout << "Using openMP for alignment.\n";
  auto const n_omp = static_cast<std::ptrdiff_t>(ci->size());
#pragma omp parallel for firstprivate(coordsReferenceStructure, coordsTemporaryStructure) reduction(+:mean_value) shared(hold_coords_str, hold_str)
  for (std::ptrdiff_t i = 0; i < n_omp; ++i)
#else
  for (std::size_t i = 0; i < ci->size(); ++i)
#endif
  {
    if (i != static_cast<std::ptrdiff_t>(Config::get().alignment.reference_frame_num))
    {
      auto temporaryPESpoint2 = ci->PES()[i].structure.cartesian;
      coordsTemporaryStructure.set_xyz(temporaryPESpoint2);
      //Create temporary objects for current frame

      if (Config::get().alignment.traj_align_translational)
      {
        centerOfMassAlignment(coordsTemporaryStructure);
      }
      if (Config::get().alignment.traj_align_rotational)
      {
        kabschAlignment(coordsTemporaryStructure, coordsReferenceStructure, false);
      }

      if (Config::get().alignment.traj_print_bool)
      {
        if (Config::get().alignment.dist_unit == 0)
          //RMSD
        {
          std::stringstream temporaryStringstream;
          double currentRootMeanSquareDevaition = root_mean_square_deviation(coordsTemporaryStructure.xyz(), coordsReferenceStructure.xyz());
          temporaryStringstream << std::setw(13) << i << " ";
          temporaryStringstream << std::setw(13) << currentRootMeanSquareDevaition << "\n";
          mean_value += currentRootMeanSquareDevaition;
          hold_str[i] = temporaryStringstream.str();
        }
        else if (Config::get().alignment.dist_unit == 1)
          //dRMSD
        {
          std::stringstream temporaryStringstream;
          temporaryStringstream << i << " ";
          double value = (double)drmsd_calc(coordsTemporaryStructure, coordsReferenceStructure);
          temporaryStringstream << std::setw(13) << value << "\n";
          mean_value += value;
          hold_str[i] = temporaryStringstream.str();
        }
        else if (Config::get().alignment.dist_unit == 2)
          //Holm&Sander Distance
        {
          std::stringstream temporaryStringstream;
          double value = (double)holmsander_calc(coordsTemporaryStructure, coordsReferenceStructure, Config::get().alignment.holm_sand_r0);
          temporaryStringstream << std::setw(13) << i << " " << value << "\n";
          mean_value += value;
          hold_str[i] = temporaryStringstream.str();
        }
      }
      //Molecular distance measure calculation

      std::stringstream hold_coords;
      hold_coords << coordsTemporaryStructure;
      hold_coords_str[i] = hold_coords.str();
      //Formatted string-output
    }

    else if (i == static_cast<std::ptrdiff_t>(Config::get().alignment.reference_frame_num))
    {
      std::stringstream hold_coords;
      hold_coords << coordsReferenceStructure;
      hold_coords_str[i] = hold_coords.str();
      //Formatted string-output (first to array because of OpenMP parallelization)
    }
  }

  std::ofstream distance(coords::output::filename("_distances").c_str(), std::ios::app);
  std::ofstream outputstream(coords::output::filename("_aligned").c_str(), std::ios::app);

  if (Config::get().general.verbosity > 2U) std::cout << "Alignment done. Writing structures to file.\n";

  for (size_t i = 0; i < ci->size(); i++)
  {
    if (Config::get().alignment.traj_print_bool)
    {
      distance << hold_str[i];
    }
    outputstream << hold_coords_str[i];
  }
  distance << "\n";
  distance << "Mean value: " << (mean_value / (double) (ci->size() - 1)) << "\n";
  //Formatted string-output

  delete[] hold_str;
  delete[] hold_coords_str;
  //Cleaning Up
}

void pca_proc(std::unique_ptr<coords::input::format>& ci, coords::Coordinates& coords)
{
  using namespace matop;
  using namespace matop::pca;
  Matrix_Class eigenvectors, trajectory;
  std::vector<size_t> structuresToBeWrittenToFile;
  std::string additionalInformation;
  readEigenvectorsAndModes(eigenvectors, trajectory, additionalInformation);
  if (Config::get().PCA.proc_desired_start.size() > trajectory.rows() || Config::get().PCA.proc_desired_stop.size() > trajectory.rows())
  {
    std::cout << "Desired PCA-Ranges have higher dimensionality then modes. Omitting the last values.\n";
  }

  for (size_t j = 0u; j < trajectory.cols(); j++)
  {
    bool isStructureInRange = true;
    for (size_t i = 0u; i < trajectory.rows() && i < std::max(Config::get().PCA.proc_desired_stop.size(), Config::get().PCA.proc_desired_start.size()); i++)

    {
      if (i < Config::get().PCA.proc_desired_start.size())
      {
        if (trajectory(i, j) < Config::get().PCA.proc_desired_start[i])
        {
          isStructureInRange = false;
        }
      }
      if (i < Config::get().PCA.proc_desired_stop.size())
      {
        if (trajectory(i, j) > Config::get().PCA.proc_desired_stop[i])
        {
          isStructureInRange = false;
        }
      }
    }
    if (isStructureInRange)
    {
      structuresToBeWrittenToFile.push_back(j);
    }
  }

  if (Config::get().general.verbosity >= 3u) std::cout << "Found " << structuresToBeWrittenToFile.size() << " structures in desired range.\n";
  if (Config::get().general.verbosity > 2u) std::cout << "Found " << structuresToBeWrittenToFile.size() << " structures in desired range.\n";

  //Undoing PCA
  trajectory = eigenvectors * trajectory;

  std::ofstream outstream(coords::output::filename("_pca_selection").c_str(), std::ios::app);

  // Case: Cartesian Coordinates.
  if (additionalInformation.substr(0, 3) == "car")
  {
    //Additional Information Processing -> Read "DOFS that were used" from file. Put their identifying numbers in vector.
    std::stringstream ss(additionalInformation.substr(4));
    std::string buffer;
    std::vector<size_t> tokens;
    std::deque<bool> alreadyFoundStructures(ci->size(), false);

    while (ss >> buffer) tokens.push_back((size_t)std::stoi(buffer));

    if (tokens.size() != 0u)
    {
      ::matop::undoMassweight(trajectory, coords, false, tokens);
      for (size_t i = 0u; i < structuresToBeWrittenToFile.size(); i++)
      {
        Matrix_Class out_mat(3, trajectory.rows() / 3u);
        for (size_t j = 0u; j < trajectory.rows(); j = j + 3)
        {
          out_mat(0, j / 3u) = trajectory(j, structuresToBeWrittenToFile[i]);
          out_mat(1, j / 3u) = trajectory(j + 1u, structuresToBeWrittenToFile[i]);
          out_mat(2, j / 3u) = trajectory(j + 2u, structuresToBeWrittenToFile[i]);
        }
        coords::Coordinates current(coords);

        // For every partial (truncated) structure that is inside the user-defined
        // range regarding its PCA-Modes,
        // we now search the matching full structure in the input trajectory.
        bool structureFound = false;
        auto structureCartesian = ci->PES()[0].structure.cartesian;
        int structureNumber = -1;

        for (size_t k = 0u; k < ci->size() && !structureFound; k++)
        {
          if (alreadyFoundStructures[k]) continue;

          structureCartesian = ci->PES()[k].structure.cartesian; //Current structure
          structureFound = true;
          structureNumber = (int)k;
          // Remeber, we are now iterating over certain atoms (those to which
          // the PCA was truncated.
          for (size_t l = 0u; l < tokens.size(); l++)
          {
            // If abs() of diff of every coordinate is smaller than 0.5% of coordinate (or, if this value
            // is very small, the arbitrary cutoff 2e-4), consider it a match.
            // However, we look for "not-matching" and break the loop. If everything matches, we continue.
            // Thats why we negate the criterion in the if clause (!)
            float_type xCompare = 0.005 * std::max(std::abs(structureCartesian[tokens[l]].x()), 2e-4);
            float_type yCompare = 0.005 * std::max(std::abs(structureCartesian[tokens[l]].y()), 2e-4);
            float_type zCompare = 0.005 * std::max(std::abs(structureCartesian[tokens[l]].z()), 2e-4);

            if (!(std::abs(out_mat(0, l) - structureCartesian[tokens[l]].x()) <= xCompare &&
              std::abs(out_mat(1, l) - structureCartesian[tokens[l]].y()) <= yCompare &&
              std::abs(out_mat(2, l) - structureCartesian[tokens[l]].z()) <= zCompare))
            {
              structureFound = false;
              break;
            }
          }
        }
        if (structureFound)
        {
          // Horray, we found it, now write it out!
          alreadyFoundStructures[structureNumber] = true;
          current.set_xyz(structureCartesian);
          current.to_internal();
          outstream << current;
        }
        else
        {
          std::cout << "Could not find structure restored from PCA-Modes in ensemble of structures from original coordinates.\n";
          std::cout << "This means that there was no provided structure with a deviance of less than 0.5% to the current restored structure.\n\n";
        }
      }
    }
    else
    {
      // Here, we merely restore the coordinates from the PCA-modes
      // since no truncation took place, and write them out.
      ::matop::undoMassweight(trajectory, coords, false);
      for (size_t i = 0u; i < structuresToBeWrittenToFile.size(); i++)
      {
        Matrix_Class out_mat(3, trajectory.rows() / 3u);
        for (size_t j = 0u; j < trajectory.rows(); j = j + 3)
        {
          out_mat(0, j / 3u) = trajectory(j, structuresToBeWrittenToFile[i]);
          out_mat(1, j / 3u) = trajectory(j + 1u, structuresToBeWrittenToFile[i]);
          out_mat(2, j / 3u) = trajectory(j + 2u, structuresToBeWrittenToFile[i]);
        }
        coords::Coordinates out(coords);
        out.set_xyz(::matop::transfer_to_3DRepressentation(out_mat));
        out.to_internal();
        outstream << out;
      }
    }
  }

  // Case: Internal Coordiantes
  else if (additionalInformation.substr(0, 3) == "int")
  {
    // [0]: bond distance tokens, [1]: bond angle tokens [2]: dihedrals tokens.
    // Here we keep track of which DOFs are to be considered
    std::deque<bool> tokens(coords.atoms().size(), false);
    std::deque<bool> alreadyFoundStructures(ci->size(), false);

    //Additional Information Processing -> Read "DOFS that were used" from file. Store in "tokens".
    std::stringstream ss(additionalInformation.substr(4));
    std::string buffer;
    // Read the additional information
    while (ss >> buffer)
    {
      if (buffer.substr(0, 1) == "d") tokens[(size_t)std::stoi(buffer.substr(1))] = true;
    }

    // This is gonna be complicated. Im sorry.
    // Iterate over structures chosen from PCA-Ensemble
    for (size_t i = 0u; i < structuresToBeWrittenToFile.size(); i++)
    {
      bool structureFound = false;
      //auto structureCartesian = ci->PES()[0].structure.cartesian;
      int structureNumber = -1;

      //Iterate over input strucutures -> find matching structure
      for (size_t k = 0u; k < ci->size() && !structureFound; k++)
      {
        if (alreadyFoundStructures[k]) continue;
        coords.set_internal(ci->PES()[k].structure.intern);
        size_t quicksearch = 0u;
        structureFound = true;
        structureNumber = (int)k;
        //Iterating over atoms, see if they all match
        for (size_t j = 0u; j < tokens.size(); j++)
        {
          // If abs() of diff of every coordinate is smaller than 0.1% of coordinate, consider it a match.
          // However, we look for "not-matching" and break the loop. If everything matches, we continue.
          // Thats why we negate the criterion in the if clause (!)
          if (tokens[j] == true)
          {
            float_type compareFromPCA1 = trajectory(quicksearch, structuresToBeWrittenToFile[i]);
            float_type compareFromTrajectory1 = std::cos(coords.intern(j).azimuth().radians());
            float_type compareFromPCA2 = trajectory(quicksearch + 1u, structuresToBeWrittenToFile[i]);
            float_type compareFromTrajectory2 = std::sin(coords.intern(j).azimuth().radians());
            bool found1 = std::abs(compareFromTrajectory1 - compareFromPCA1) <= 0.001 * std::abs(compareFromPCA1) \
              || std::abs(compareFromTrajectory1 - compareFromPCA1) < 0.0000001;
            bool found2 = std::abs(compareFromTrajectory2 - compareFromPCA2) <= 0.001 * std::abs(compareFromPCA2) \
              || std::abs(compareFromTrajectory2 - compareFromPCA2) < 0.0000001;
            //If structures did not match
            if (!(found1 && found2))
            {
              structureFound = false;
              break;
            }
            quicksearch += 2u;
          }
        }
        //If match was found, write out.
        if (structureFound)
        {
          alreadyFoundStructures[structureNumber] = true;
          coords.set_pes(ci->PES()[k]);
          outstream << coords;
        }
      }
      if (!structureFound)
      {
        std::cout << "Could not find structure restored from PCA-Modes in ensemble of structures from original coordinates.\n";
        std::cout << "You probably made a mistake somewhere in your INPUTFILE.\nDo not consider structures written out after this message as valid.\n";
      }
    }
  }
}
