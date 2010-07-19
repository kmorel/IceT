## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

# These source files are actually compiled in the Core IceT source
# directory.  They are separated in a separate directory for convenience.
ADD_CUSTOM_TARGET(strategies_target ALL
  DEPENDS IceTCore
  )
