# civetkern  

civet的数据库，以lmdb作为存储引擎，在此基础上开发的带有搜索功能的业务数据库。  

为了增强搜索功能，后续计划引入图结构，并参考lemongraph的实现，重构数据库，实现业务和数据库的分离

因为搜索会将相似的内容检索出来，因此要尽可能把相似的节点聚类到同一个页中。计划采用无标度网络模型