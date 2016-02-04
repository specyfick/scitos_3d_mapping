#ifndef MODELDATABASERETRIEVAL_H
#define MODELDATABASERETRIEVAL_H

#include "ModelDatabase.h"
#include <vocabulary_tree/vocabulary_tree.h>
#include <dynamic_object_retrieval/visualize.h>

using HistT = pcl::Histogram<250>;
using HistCloudT = pcl::PointCloud<HistT>;

class ModelDatabaseRetrieval: public ModelDatabase{
private:

    vocabulary_tree<HistT, 8> vt;
    vector<HistCloudT::Ptr> vt_features;
    map<int, int> added_indices;
    int training_indices;

public:
    virtual int add(pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud, double weight = 1);

    // return true if successfull
    // return false if fail
    virtual bool remove(int index);

    //Find the number_of_matches closest matches in dabase to the pointcloud for index
    virtual std::vector<int> search(int index, int number_of_matches);

    ModelDatabaseRetrieval();
    ~ModelDatabaseRetrieval();
};

#endif // MODELDATABASERETRIEVAL_H
