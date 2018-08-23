/**
CAST 3
PrimitiveInternalCoordinates.h
Purpose: Definition of primitive Internal Coordinate Systems


@author Julian Erdmannsdörfer, Michael Prem
@version 3.0
*/

#ifndef PRIMITIVE_INTERNAL_COORDINATES_H
#define PRIMITIVE_INTERNAL_COORDINATES_H

#include"InternalCoordinates.h"
#include"coords.h"
#include "ic_core.h"
#include "graph.h"

namespace internals {
  class PrimitiveInternalCoordinates {
  protected:
    using CartesianType = InternalCoordinates::CartesiansForInternalCoordinates;
    using BondGraph = ic_util::Graph<ic_util::Node>;
    using InternalVec = std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>>;
  public:
    PrimitiveInternalCoordinates(const std::vector<coords::Representation_3D>& res_init,
      const std::vector<std::vector<std::size_t>>& res_index,
      CartesianType & xyz_init, BondGraph const& graph)
      : res_vec_{ res_init }, subSystemIndices{ res_index } {
      create_ic_system(graph, xyz_init);
    }
    PrimitiveInternalCoordinates() = default;
    virtual ~PrimitiveInternalCoordinates() = default;

    std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>> primitive_internals;
    //std::vector<std::shared_ptr<InternalCoordinates::Rotator>> rotation_vec_;

    void requestNewBAndG(){
     new_B_matrix = true;
     new_G_matrix = true;
    }

  protected:

    const std::vector<coords::Representation_3D> res_vec_;
    const std::vector<std::vector<std::size_t>> subSystemIndices;
    //CartesianType xyz_;
    std::vector<std::shared_ptr<InternalCoordinates::Rotator>> registeredRotators;

    scon::mathmatrix<coords::float_type> B_matrix;
    scon::mathmatrix<coords::float_type> G_matrix;
    scon::mathmatrix<coords::float_type> hessian;

    std::vector<std::vector<coords::float_type>> deriv_vec(CartesianType const& cartesians);
    

    static std::vector<std::vector<std::size_t>> possible_sets_of_3(BondGraph::adjacency_iterator const vbegin, BondGraph::adjacency_iterator const vend);
    std::shared_ptr<InternalCoordinates::Rotator> build_rotation(InternalCoordinates::CartesiansForInternalCoordinates & target,
      std::vector<std::size_t> const& index_vec);

    bool new_B_matrix = true;
    bool new_G_matrix = true;

    class InternalCoordinatesCreator {
    protected:
      using BondGraph = ic_util::Graph<ic_util::Node>;
    public:
      InternalCoordinatesCreator(BondGraph const& graph) : bondGraph{ graph } {}
      virtual ~InternalCoordinatesCreator() = default;
      virtual std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>> getInternals() = 0;
    protected:
      ic_util::Graph<ic_util::Node> const& bondGraph;
    };

    class DistanceCreator : public InternalCoordinatesCreator {
    public:
      DistanceCreator(BondGraph const& graph) : InternalCoordinatesCreator{ graph }, source{ 0u }, target{ 0u }, edgeIterators{ boost::edges(bondGraph) } {}
      virtual ~DistanceCreator() = default;

      virtual std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>> getInternals() override;

    protected:
      bool nextEdgeDistances();
      std::size_t source, target;
      std::pair<ic_util::Graph<ic_util::Node>::edge_iterator, ic_util::Graph<ic_util::Node>::edge_iterator> edgeIterators;
    };

    class AngleCreator : public InternalCoordinatesCreator {
    public:
      AngleCreator(BondGraph const& graph) : InternalCoordinatesCreator{ graph }, leftAtom{ 0u }, middleAtom{ 0u }, rightAtom{ 0u }, vertexIterators{ boost::vertices(graph) } {}
      virtual ~AngleCreator() = default;

      virtual std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>> getInternals() override;
    protected:
      bool nextVertex();
      void addAngleForAllNeighbors();
      void spanLeftAndRightNeighborsForAngle(std::pair<BondGraph::adjacency_iterator, BondGraph::adjacency_iterator> & neighbors);
      bool findLeftAtom(std::pair<BondGraph::adjacency_iterator, BondGraph::adjacency_iterator> & neighbors);

      bool findRightAtom(std::pair<BondGraph::adjacency_iterator, BondGraph::adjacency_iterator> & neighborsLeft);

      std::size_t leftAtom, middleAtom, rightAtom;
      std::pair<ic_util::Graph<ic_util::Node>::vertex_iterator, ic_util::Graph<ic_util::Node>::vertex_iterator> vertexIterators;
      std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>> * pointerToResult;
    };

    class DihedralCreator : public DistanceCreator {
    public:
      DihedralCreator(BondGraph const& graph) : DistanceCreator{ graph }, outerLeft{ 0u }, outerRight{ 0u } {}
      virtual ~DihedralCreator() = default;

      virtual std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>> getInternals() override;
    protected:
      void findLeftAndRightAtoms();
      bool findLeftAtoms(std::pair<BondGraph::adjacency_iterator, BondGraph::adjacency_iterator> & sourceNeighbors);
      bool findRightAtoms(std::pair<BondGraph::adjacency_iterator, BondGraph::adjacency_iterator> & targetNeighbors);

      std::size_t outerLeft, outerRight;

      std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>> * pointerToResult;
    };

  public:
    void append_primitives(InternalVec && pic);

    InternalVec create_distances(BondGraph const&) const;
    InternalVec create_angles(BondGraph const&) const;
    InternalVec create_oops(coords::Representation_3D const&, BondGraph const&) const;
    std::vector<std::unique_ptr<InternalCoordinates::InternalCoordinate>> create_dihedrals(BondGraph const&) const;

    InternalVec create_trans_x() const;
    InternalVec create_trans_y() const;
    InternalVec create_trans_z() const;
    std::tuple<InternalVec, InternalVec, InternalVec>
      create_translations()const;

    std::tuple<InternalVec, InternalVec, InternalVec>
      createRotationABC(std::vector<InternalCoordinates::Rotations> & rotations);
    std::tuple<InternalVec, InternalVec, InternalVec>
      create_rotations(CartesianType & cartesians);

    void create_ic_system(BondGraph const&, CartesianType &);

    virtual scon::mathmatrix<coords::float_type> calc(coords::Representation_3D const& xyz) const;//F
    virtual scon::mathmatrix<coords::float_type> calc_diff(coords::Representation_3D const& lhs, coords::Representation_3D const& rhs) const;//F

    virtual scon::mathmatrix<coords::float_type> guess_hessian(CartesianType const&) const;//F
    virtual scon::mathmatrix<coords::float_type>& Bmat(CartesianType const& cartesians);//F
    virtual scon::mathmatrix<coords::float_type>& Gmat(CartesianType const& cartesians);//F
    virtual scon::mathmatrix<coords::float_type> transposeOfBmat(CartesianType const& cartesian);
    virtual scon::mathmatrix<coords::float_type> pseudoInverseOfGmat(CartesianType const& cartesian);

  };

  /*class EnergyStep : public scon::mathmatrix<coords::float_type> {
  public:
    using scon::mathmatrix<coords::float_type>::mathmatrix;
    using scon::mathmatrix<coords::float_type>::operator=;
    virtual ~EnergyStep() = default;
    virtual EnergyStep & takeStep(scon::mathmatrix<coords::float_type> const&, scon::mathmatrix<coords::float_type> const&) = 0;
  protected:
    virtual void invertNormalHessian(scon::mathmatrix<double> const& hessian) = 0;
    std::unique_ptr<scon::mathmatrix<coords::float_type> > inverseHessian;
  };

  class InternalStep : public EnergyStep {
  public:
    using EnergyStep::EnergyStep;
    using EnergyStep::operator=;
    EnergyStep & takeStep(scon::mathmatrix<coords::float_type> const&, scon::mathmatrix<coords::float_type> const&) override;
  protected:
    void invertNormalHessian(scon::mathmatrix<double> const& hessian) override;
  };*/

  class InternalToCartesianConverter {
  public:
    InternalToCartesianConverter(PrimitiveInternalCoordinates & internals,
      InternalCoordinates::CartesiansForInternalCoordinates & cartesians) : internalCoordinates{ internals }, cartesianCoordinates{ cartesians }, inverseHessian(0u,0u) {}
    virtual ~InternalToCartesianConverter() = default;

    scon::mathmatrix<coords::float_type> calculateInternalGradients(scon::mathmatrix<coords::float_type> const&);
    virtual scon::mathmatrix<coords::float_type> getInternalStep(scon::mathmatrix<coords::float_type> const&, scon::mathmatrix<coords::float_type> const&);

    virtual std::pair<coords::float_type, coords::float_type> getDeltaYPrimeAndSol(scon::mathmatrix<coords::float_type> const& internalStep, scon::mathmatrix<coords::float_type> const& gradients, scon::mathmatrix<coords::float_type> const& hessian);
    virtual void applyInternalChange(scon::mathmatrix<coords::float_type>);//TODO soll kartesische Koordinaten als mathmatrix zurück geben, überschreiben, erst nachdem der Optimierungsschritt zuende ist
    virtual coords::Representation_3D applyInternalChange(scon::mathmatrix<coords::float_type>) const;
    template<typename XYZ>
    coords::Representation_3D& set_xyz(XYZ&& new_xyz);
    virtual InternalCoordinates::CartesiansForInternalCoordinates const& getCartesianCoordinates() const { return cartesianCoordinates; }
    virtual InternalCoordinates::CartesiansForInternalCoordinates & getCartesianCoordinates() { return cartesianCoordinates; }
    
    void invertNormalHessian(scon::mathmatrix<double> const& hessian);

    std::pair<coords::float_type, coords::float_type> cartesianNormOfOtherStructureAndCurrent(coords::Representation_3D const& otherCartesians) const;
  protected:
    PrimitiveInternalCoordinates & internalCoordinates;
    InternalCoordinates::CartesiansForInternalCoordinates & cartesianCoordinates;
    //write in another class so that it gets deleted after used by all functions
    scon::mathmatrix<coords::float_type> inverseHessian;

    coords::float_type getDeltaYPrime(scon::mathmatrix<coords::float_type> const& internalStep);
    coords::float_type InternalToCartesianConverter::getSol(scon::mathmatrix<coords::float_type> const& internalStep, scon::mathmatrix<coords::float_type> const& gradients, scon::mathmatrix<coords::float_type> const& hessian);
  private:
    template<typename Dcart>
    coords::Representation_3D& takeCartesianStep(Dcart&& d_cart);
    void takeCartesianStep(scon::mathmatrix <coords::float_type> && cartesianChange, InternalCoordinates::temporaryCartesian & cartesians) const;
  };

  class AppropriateStepFinder;

  class StepRestrictor {
  public:
    StepRestrictor(scon::mathmatrix<coords::float_type> * step, coords::Representation_3D * cartesians, coords::float_type const target) : stepCallbackReference{ step }, cartesianCallbackReference{ cartesians }, target{ target }, restrictedStep{}, restrictedSol{ 0.0 }, v0{ 0.0 } {}
    virtual ~StepRestrictor() = default;
    virtual coords::float_type operator()(AppropriateStepFinder & finder);

    coords::float_type getRestrictedSol() const { return restrictedSol; }
    virtual scon::mathmatrix<coords::float_type> const& getRestrictedStep() const { return restrictedStep; }
    virtual scon::mathmatrix<coords::float_type> & getRestrictedStep() { return restrictedStep; }

    void setCartesians(coords::Representation_3D && cartesians) {
      correspondingCartesians = std::move(cartesians);
    }
    coords::Representation_3D const& getCartesians() const { return correspondingCartesians; }
    void setInitialV0(coords::float_type const initialV0) { v0 = initialV0; }
    bool targetIsZero() const { return target == 0.0; }
    coords::float_type getTarget() const { return target; }
    void registerBestGuess() { 
      *stepCallbackReference = std::move(restrictedStep);
      *cartesianCallbackReference = std::move(correspondingCartesians);
    }
  protected:
    scon::mathmatrix<coords::float_type> alterHessian(scon::mathmatrix<coords::float_type> const & hessian, coords::float_type const alteration) const;
    coords::float_type getStepNorm() const { return restrictedStep.norm(); }

    scon::mathmatrix<coords::float_type> * stepCallbackReference;//TODO make these pointers to shared pointer
    coords::Representation_3D * cartesianCallbackReference;
    coords::float_type target;
    scon::mathmatrix<coords::float_type> restrictedStep;
    coords::Representation_3D correspondingCartesians;
    coords::float_type restrictedSol, v0;
  };

  class StepRestrictorFactory {
  public:
    StepRestrictorFactory(AppropriateStepFinder & finder);
    StepRestrictor makeStepRestrictor(coords::float_type const target) {
      return StepRestrictor{ finalStep, finalCartesians, target };
    }
  private:
    scon::mathmatrix<coords::float_type> * const finalStep;
    coords::Representation_3D * const finalCartesians;
  };

  class InternalToCartesianStep {
  public:
    InternalToCartesianStep(AppropriateStepFinder & finder, coords::float_type const trustRadius)
      : finder{ finder }, trustRadius {trustRadius} {}
    virtual ~InternalToCartesianStep() = default;

    virtual coords::float_type operator()(StepRestrictor & restrictor);
    
  protected:
    AppropriateStepFinder & finder;
    coords::float_type trustRadius;
  };

  class BrentsMethod {
  public:
    BrentsMethod(AppropriateStepFinder & finder, coords::float_type const leftLimit, coords::float_type const rightLimit, coords::float_type const trustStep)
      : finder{ finder }, leftLimit{ leftLimit }, middle{ 0.0 }, oldMiddle{ 0.0 }, rightLimit{ rightLimit }, result{ 0.0 },
      trustStep{ trustStep }, threshold{ 0.1 }, delta{ 1.e-6 }, bisectionWasUsed{ true } {}

    coords::float_type operator()(InternalToCartesianStep & internalToCartesianStep);
  protected:
    bool useBisection()const;

    AppropriateStepFinder & finder;
    coords::float_type leftLimit, middle, oldMiddle, rightLimit, result;
    coords::float_type const trustStep;
    coords::float_type const threshold;
    coords::float_type const delta;
    bool bisectionWasUsed;
  };
  
  class AppropriateStepFinder {
  public:
    AppropriateStepFinder(InternalToCartesianConverter const& converter, scon::mathmatrix<coords::float_type> const& gradients, scon::mathmatrix<coords::float_type> const& hessian) : 
      converter{ converter }, gradients { gradients }, hessian{ hessian }, inverseHessian{ hessian.pinv() }, bestStepSoFar{}, stepRestrictorFactory{ *this } {}
    //InternalToCartesianConverter & converter;
    scon::mathmatrix<coords::float_type> const& gradients;
    scon::mathmatrix<coords::float_type> const& hessian;
    scon::mathmatrix<coords::float_type> inverseHessian;


    virtual void appropriateStep(coords::float_type const trustRadius);

    virtual coords::float_type getDeltaYPrime(scon::mathmatrix<coords::float_type> const& internalStep) const;
    virtual coords::float_type getSol(scon::mathmatrix<coords::float_type> const& internalStep) const;
    virtual scon::mathmatrix<coords::float_type> getInternalStep() const;
    virtual scon::mathmatrix<coords::float_type> getInternalStep(scon::mathmatrix<coords::float_type> const& hessian) const;
    coords::Representation_3D & getCartesians() { return bestCartesiansSoFar; }
    StepRestrictor generateStepRestrictor(coords::float_type const target) {
      return stepRestrictorFactory.makeStepRestrictor(target);
    }
    virtual coords::float_type applyInternalChangeAndGetNorm(StepRestrictor & internalStep);
    virtual coords::float_type applyInternalChangeAndGetNorm(scon::mathmatrix<coords::float_type> const& internalStep);
    virtual scon::mathmatrix<coords::float_type> alterHessian(coords::float_type const alteration) const;
  protected:

    friend class StepRestrictorFactory;

    scon::mathmatrix<coords::float_type> * getAddressOfInternalStep() { return std::addressof(bestStepSoFar); }
    coords::Representation_3D * getAddressOfCartesians() { return std::addressof(bestCartesiansSoFar); }

    InternalToCartesianConverter const& converter;
    scon::mathmatrix<coords::float_type> bestStepSoFar;
    coords::Representation_3D bestCartesiansSoFar;
    StepRestrictorFactory stepRestrictorFactory;
  };

  


  template<typename Dcart>
  coords::Representation_3D& InternalToCartesianConverter::takeCartesianStep(Dcart&& d_cart) {
    auto d_cart_rep3D = ic_util::mat_to_rep3D(std::forward<Dcart>(d_cart));
    return set_xyz(cartesianCoordinates + d_cart_rep3D);
  }

  template<typename XYZ>
  coords::Representation_3D& InternalToCartesianConverter::set_xyz(XYZ&& new_xyz) {
    internalCoordinates.requestNewBAndG();
    return cartesianCoordinates.setCartesianCoordnates(std::forward<XYZ>(new_xyz));
  }
}

#endif
