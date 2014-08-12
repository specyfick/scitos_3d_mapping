#include "fine_mapping.h"

#include "fine_registration.h"
#include "asynch_visualizer.h"
#include "fine_edge.h"
#include "fine_vertex.h"

#include <Eigen/Dense>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/registration/icp.h>
#include <pcl/filters/filter.h>
#include <pcl/filters/voxel_grid.h>

#include "g2o/core/sparse_optimizer.h"
#include "g2o/core/block_solver.h"
#include "g2o/core/linear_solver.h"
#include "g2o/core/optimization_algorithm_levenberg.h"
#include "g2o/solvers/pcg/linear_solver_pcg.h"
#include "g2o/types/slam3d/edge_se3.h"
#include "g2o/types/slam3d/vertex_se3.h"

bool FineMapping::register_clouds_icp(Eigen::Matrix3f& R, Eigen::Vector3f& t,
                         scan* scan1, scan* scan2,
                         pcl::PointCloud<pcl::PointXYZRGB>::Ptr& cloud1, pcl::PointCloud<pcl::PointXYZRGB>::Ptr& cloud2)
{
    Eigen::Matrix3f R1, R2;
    Eigen::Vector3f t1, t2;
    scan1->get_transform(R1, t1);
    scan2->get_transform(R2, t2);
    Eigen::Matrix3f Rdelta = R1.transpose()*R2;
    Eigen::Vector3f tdelta = R1.transpose()*(t2 - t1);
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr  cloudt(new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::copyPointCloud(*cloud2, *cloudt);
    for (pcl::PointXYZRGB& p : cloudt->points) {
        p.getVector3fMap() = Rdelta*p.getVector3fMap()+tdelta;
    }
    pcl::IterativeClosestPoint<pcl::PointXYZRGB, pcl::PointXYZRGB> icp; // don't really need rgb
    // Set the max correspondence distance to 5cm (e.g., correspondences with higher distances will be ignored)
    icp.setMaxCorrespondenceDistance(0.03);
    // Set the maximum number of iterations (criterion 1)
    icp.setMaximumIterations(50);
    // Set the transformation epsilon (criterion 2)
    icp.setTransformationEpsilon(1e-12);
    // Set the euclidean distance difference epsilon (criterion 3)
    icp.setEuclideanFitnessEpsilon(1);
    icp.setInputSource(cloud1);
    icp.setInputTarget(cloudt);
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr final(new pcl::PointCloud<pcl::PointXYZRGB>);
    icp.align(*final);
    std::cout << "has converged:" << icp.hasConverged() << " score: " <<
                 icp.getFitnessScore() << std::endl;
    Eigen::Matrix4f T = icp.getFinalTransformation();
    T /= T(3, 3);
    Eigen::Matrix3f Rcomp = T.topLeftCorner<3, 3>();
    Eigen::Vector3f tcomp = T.block<3, 1>(0, 3);
    //R = Rdelta.transpose()*Rcomp;
    //t = Rdelta*(tcomp - tdelta);
    R = Rdelta*Rcomp.transpose();
    t = tdelta - Rdelta*Rcomp.transpose()*tcomp;

    if (icp.hasConverged()) {
        return true;
    }
    else {
        return false;
    }
}

void FineMapping::compute_initial_transformation(Eigen::Matrix3f& R, Eigen::Vector3f& t, scan* scan1, scan* scan2)
{
    Eigen::AngleAxisf a(R);
    if (a.angle() < 0.06 && t.norm() < 0.1) {
        return;
    }
    Eigen::Matrix3f R1, R2;
    Eigen::Vector3f t1, t2;
    scan1->get_transform(R1, t1);
    scan2->get_transform(R2, t2);
    R = R1.transpose()*R2;
    t = R1.transpose()*(t2-t1);
}


int FineMapping::fineMapping(std::vector<scan*> scans , std::vector<CloudPtr> allClouds)
{
    typedef g2o::BlockSolver<g2o::BlockSolverTraits<-1, -1> >  SlamBlockSolver;
    typedef g2o::LinearSolverPCG<SlamBlockSolver::PoseMatrixType> SlamLinearSolver;

    g2o::SparseOptimizer optimizer;
    SlamLinearSolver* linearSolver = new SlamLinearSolver();
    SlamBlockSolver* blockSolver = new SlamBlockSolver(linearSolver);
    g2o::OptimizationAlgorithmLevenberg* solver = new g2o::OptimizationAlgorithmLevenberg(blockSolver);
    solver->setUserLambdaInit(5); // 0
    optimizer.setAlgorithm(solver);

    int n = scans.size();

    // adding the odometry to the optimizer
    // first adding all the vertices
    Eigen::Matrix3f R;
    Eigen::Vector3f t;
    Eigen::Matrix4d T;
    T.setIdentity();
    std::vector<Eigen::Isometry3d, Eigen::aligned_allocator<Eigen::Isometry3d> > estimates;
    std::cout << "Optimization: Adding robot poses ... ";
    for (size_t i = 0; i < n; ++i) {
        scans[i]->get_transform(R, t);
        T.topLeftCorner<3, 3>() = R.cast<double>();
        T.block<3, 1>(0, 3) = t.cast<double>();
        Eigen::Isometry3d transform(T);
        estimates.push_back(transform);
    }
    std::cout << "done." << std::endl;

    typedef std::pair<size_t, size_t> edge_pair;
    std::vector<edge_pair> pairs;
    std::vector<Eigen::Isometry3d, Eigen::aligned_allocator<Eigen::Isometry3d> > measurements;
    std::vector<bool> measurement_correct;

    bool correct;
    for (size_t i = 0; i < n; ++i) { // n
        size_t j = (i+1)%n;
        size_t k = (i+2)%n;
        if (i % 2 == 0) {
            correct = fine_registration::register_scans(R, t, scans[i], scans[j]);
            T.topLeftCorner<3, 3>() = R.cast<double>();
            T.block<3, 1>(0, 3) = t.cast<double>();
            Eigen::Isometry3d transform(T);
            pairs.push_back(edge_pair(i, j));
            measurements.push_back(transform);
            measurement_correct.push_back(correct);
        }
        correct = fine_registration::register_scans(R, t, scans[i], scans[k]);
        T.topLeftCorner<3, 3>() = R.cast<double>();
        T.block<3, 1>(0, 3) = t.cast<double>();
        Eigen::Isometry3d transform(T);
        pairs.push_back(edge_pair(i, k));
        measurements.push_back(transform);
        measurement_correct.push_back(correct);
    }

    std::vector<g2o::VertexSE3*> vertices;
    // add vertices to optimizer
    for (size_t i = 0; i < n; ++i) {
        g2o::VertexSE3* robot = new g2o::VertexSE3;
        robot->setId(i);
        robot->setEstimate(estimates[i]);
        optimizer.addVertex(robot);
        vertices.push_back(robot);
    }

    // good information
    Eigen::Matrix<double, 6, 6> good_info;
    good_info.setIdentity();
    good_info.bottomRightCorner<3, 3>() *= 100.0;

    // bad information
    Eigen::Matrix<double, 6, 6> bad_info;
    bad_info.setIdentity();
    bad_info /= 10.0;

    // second add the odometry constraints
    std::cout << "Optimization: Adding odometry measurements ... ";
    for (size_t i = 0; i < measurements.size(); ++i) {
        g2o::EdgeSE3* odometry = new g2o::EdgeSE3;
        odometry->vertices()[0] = optimizer.vertex(pairs[i].first);
        odometry->vertices()[1] = optimizer.vertex(pairs[i].second);
        odometry->setMeasurement(measurements[i]);
        if (measurement_correct[i]) {
            odometry->setInformation(good_info);
        }
        else {
            odometry->setInformation(good_info);//bad_info
        }
        optimizer.addEdge(odometry);
    }
    std::cout << "done." << std::endl;

    optimizer.initializeOptimization();
    std::cout << "Optimizing..." << std::endl;
    optimizer.setVerbose(true);
    optimizer.optimize(10);
    std::cout << "Done optimizing!" << std::endl;

    for (size_t i = 0; i < n; ++i) {
        Eigen::Isometry3d estimate = vertices[i]->estimate();
        Eigen::Matrix4f transform = estimate.matrix().cast<float>();
        R = transform.topLeftCorner<3, 3>();
        t = transform.block<3, 1>(0, 3);
        scans[i]->set_transform(R, t);
    }

    for (g2o::HyperGraph::Edge* e : optimizer.edges()) {
        g2o::EdgeSE3* es = (g2o::EdgeSE3*)e;
        std::cout << es->error().transpose() << std::endl;
    }


    // transform clouds in the global frame of reference
    for (size_t i = 0; i < n; ++i) {

        scans[i]->get_transform(R, t);
        for (pcl::PointXYZRGB& point : allClouds[i]->points) {
            point.getVector3fMap() = R*point.getVector3fMap() + t;
        }
    }

    return 0;
}