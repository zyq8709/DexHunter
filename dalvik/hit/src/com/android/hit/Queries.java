/*
 * Copyright (C) 2008 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.hit;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

public class Queries {
    /*
     * NOTES:  Here's a list of the queries that can be done in hat and
     * how you'd perform a similar query here in hit:
     *
     * hat                      hit
     * ------------------------------------------------------------------------
     * allClasses               classes
     * allClassesWithPlatform   allClasses
     * class                    findClass
     * instances                instancesOf
     * allInstances             allInstancesOf
     * object                   findObject
     * showRoots                getRoots
     * newInstances             newInstances
     *
     * reachableFrom            make a call to findObject to get the target
     *                          parent object, this will give you an Instance.
     *                          Then call visit(Set, Filter) on that to have
     *                          it build the set of objects in its subgraph.
     *
     * rootsTo                  make a call to findObject on the leaf node
     *                          in question, this will give you an Instance.
     *                          Instances have an ArrayList of all of the
     *                          parent objects that refer to it.  You can
     *                          follow those parent links until you hit an
     *                          object whose parent is null or a ThreadObj.
     *                          You've not successfully traced the paths to
     *                          the roots.
     */

    private static final String DEFAULT_PACKAGE = "<default>";

    /*
     * Produce a collection of all classes, broken down by package.
     * The keys of the resultant map iterate in sorted package order.
     * The values of the map are the classes defined in each package.
     */
    public static Map<String, Set<ClassObj>> allClasses(State state) {
        return classes(state, null);
    }

    public static Map<String, Set<ClassObj>> classes(State state,
            String[] excludedPrefixes) {
        TreeMap<String, Set<ClassObj>> result =
        new TreeMap<String, Set<ClassObj>>();

        Set<ClassObj> classes = new TreeSet<ClassObj>();

        //  Build a set of all classes across all heaps
        for (Heap heap: state.mHeaps.values()) {
            classes.addAll(heap.mClassesById.values());
        }

        //  Filter it if needed
        if (excludedPrefixes != null) {
            final int N = excludedPrefixes.length;
            Iterator<ClassObj> iter = classes.iterator();

            while (iter.hasNext()) {
                ClassObj theClass = iter.next();
                String classPath = theClass.toString();

                for (int i = 0; i < N; i++) {
                    if (classPath.startsWith(excludedPrefixes[i])) {
                        iter.remove();
                        break;
                    }
                }
            }
        }

        //  Now that we have a final list of classes, group them by package
        for (ClassObj theClass: classes) {
            String packageName = DEFAULT_PACKAGE;
            int lastDot = theClass.mClassName.lastIndexOf('.');

            if (lastDot != -1) {
                packageName = theClass.mClassName.substring(0, lastDot);
            }

            Set<ClassObj> classSet = result.get(packageName);

            if (classSet == null) {
                classSet = new TreeSet<ClassObj>();
                result.put(packageName, classSet);
            }

            classSet.add(theClass);
        }

        return result;
    }

    /*
     * It's sorta sad that this is a pass-through call, but it seems like
     * having all of the hat-like query methods in one place is a good thing
     * even if there is duplication of effort.
     */
    public static ClassObj findClass(State state, String name) {
        return state.findClass(name);
    }

    /*
     * Return an array of instances of the given class.  This does not include
     * instances of subclasses.
     */
     public static Instance[] instancesOf(State state, String baseClassName) {
         ClassObj theClass = state.findClass(baseClassName);

         if (theClass == null) {
             throw new IllegalArgumentException("Class not found: "
                + baseClassName);
         }

         Instance[] instances = new Instance[theClass.mInstances.size()];

         return theClass.mInstances.toArray(instances);
     }

    /*
     * Return an array of instances of the given class.  This includes
     * instances of subclasses.
     */
    public static Instance[] allInstancesOf(State state, String baseClassName) {
        ClassObj theClass = state.findClass(baseClassName);

        if (theClass == null) {
            throw new IllegalArgumentException("Class not found: "
                + baseClassName);
        }

        ArrayList<ClassObj> classList = new ArrayList<ClassObj>();

        classList.add(theClass);
        classList.addAll(traverseSubclasses(theClass));

        ArrayList<Instance> instanceList = new ArrayList<Instance>();

        for (ClassObj someClass: classList) {
            instanceList.addAll(someClass.mInstances);
        }

        Instance[] result = new Instance[instanceList.size()];

        instanceList.toArray(result);

        return result;
    }

    private static ArrayList<ClassObj> traverseSubclasses(ClassObj base) {
        ArrayList<ClassObj> result = new ArrayList<ClassObj>();

        for (ClassObj subclass: base.mSubclasses) {
            result.add(subclass);
            result.addAll(traverseSubclasses(subclass));
        }

        return result;
    }

    /*
     * Find a reference to an object based on its id.  The id should be
     * in hexadecimal.
     */
    public static Instance findObject(State state, String id) {
        long id2 = Long.parseLong(id, 16);

        return state.findReference(id2);
    }

    public static Collection<RootObj> getRoots(State state) {
        HashSet<RootObj> result = new HashSet<RootObj>();

        for (Heap heap: state.mHeaps.values()) {
            result.addAll(heap.mRoots);
        }

        return result;
    }

    public static final Instance[] newInstances(State older, State newer) {
        ArrayList<Instance> resultList = new ArrayList<Instance>();

        for (Heap newHeap: newer.mHeaps.values()) {
            Heap oldHeap = older.getHeap(newHeap.mName);

            if (oldHeap == null) {
                continue;
            }

            for (Instance instance: newHeap.mInstances.values()) {
                Instance oldInstance = oldHeap.getInstance(instance.mId);

                /*
                 * If this instance wasn't in the old heap, or was there,
                 * but that ID was for an obj of a different type, then we have
                 * a newly allocated object and we should report it in the
                 * results.
                 */
                if ((oldInstance == null)
                        || (instance.mClassId != oldInstance.mClassId)) {
                    resultList.add(instance);
                }
            }
        }

        Instance[] resultArray = new Instance[resultList.size()];

        return resultList.toArray(resultArray);
    }
}
