DKAN Datastore CartoDB Integration
======================
![screen shot 2014-06-17 at 2 46 57 pm](https://cloud.githubusercontent.com/assets/512243/3304996/d5e9e254-f650-11e3-8adc-56b110754e36.png)

This module extends the DKAN Datastore to provide CartoDB integration.

When enabled the "Datastore" functionality is completely offloaded to CartoDB.

Files are uploaded to DKAN and posted to CartodB and stored as tables.

The CartoDB SQL API is made available for querying instead of the native DKAN Datastore API.

Installation
============
This module requires the CartoDB PHP library: https://github.com/Vizzuality/cartodbclient-php

It should be downloaded and placed in ``sites/all/libraries``. The ``cartodb.class.php`` file should be located at ``sites/all/libraries/cartodbclient-php/cartodb.class.php``

You can also use the dkan_datastore_cartodb.make file: ``drush make --no-core dkan_datastore_cartodb.make``

Configuration and Use
====================

See this blog post for details: http://www.nuams.com/blog/publish-beautiful-maps-cartodb-and-dkan
